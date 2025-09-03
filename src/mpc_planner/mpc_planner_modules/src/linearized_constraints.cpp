#include "mpc_planner_modules/linearized_constraints.h"

#include <mpc_planner_solver/mpc_planner_parameters.h>

#include <mpc_planner_util/parameters.h>
#include <mpc_planner_util/data_visualization.h>

#include <ros_tools/profiling.h>

#include <algorithm>

namespace MPCPlanner
{
  LinearizedConstraints::LinearizedConstraints(std::shared_ptr<Solver> solver)
      : ControllerModule(ModuleType::CONSTRAINT, solver, "linearized_constraints")
  {
    LOG_INITIALIZE("Linearized Constraints");
    _n_discs = CONFIG["n_discs"].as<int>(); // Is overwritten to 1 for topology constraints

    _n_other_halfspaces = CONFIG["linearized_constraints"]["add_halfspaces"].as<int>();
    _max_obstacles = CONFIG["max_obstacles"].as<int>();
    int n_constraints = _max_obstacles + _n_other_halfspaces;
    _a1.resize(CONFIG["n_discs"].as<int>());
    _a2.resize(CONFIG["n_discs"].as<int>());
    _b.resize(CONFIG["n_discs"].as<int>());
    for (int d = 0; d < CONFIG["n_discs"].as<int>(); d++)
    {
      _a1[d].resize(CONFIG["N"].as<int>());
      _a2[d].resize(CONFIG["N"].as<int>());
      _b[d].resize(CONFIG["N"].as<int>());
      for (int k = 0; k < CONFIG["N"].as<int>(); k++)
      {
        _a1[d][k] = Eigen::ArrayXd(n_constraints);
        _a2[d][k] = Eigen::ArrayXd(n_constraints);
        _b[d][k] = Eigen::ArrayXd(n_constraints);
      }
    }

    _num_obstacles = 0;
    LOG_INITIALIZED();
  }

  void LinearizedConstraints::setTopologyConstraints()
  {
    _n_discs = 1; // Only one disc is used for the topology constraints
    _use_guidance = true;
  }

  void LinearizedConstraints::update(State &state, const RealTimeData &data, ModuleData &module_data)
  {
    (void)state;
    LOG_MARK("LinearizedConstraints::update");

    _dummy_b = state.get("x") + 100.;

    // Thread safe
    std::vector<DynamicObstacle> copied_obstacles = data.dynamic_obstacles;
    _num_obstacles = copied_obstacles.size();

    // For all stages
    for (int k = 1; k < _solver->N; k++)
    {
      for (int d = 0; d < _n_discs; d++)
      {
        Eigen::Vector2d pos(_solver->getEgoPrediction(k, "x"), _solver->getEgoPrediction(k, "y")); // k = 0 is initial state

        if (!_use_guidance) // Use discs and their positions
        {
          auto &disc = data.robot_area[d];

          Eigen::Vector2d disc_pos = disc.getPosition(pos, _solver->getEgoPrediction(k, "psi"));
          projectToSafety(copied_obstacles, k, disc_pos); // Ensure that the vehicle position is collision-free

          /** @todo Set projected disc position */

          pos = disc_pos;
        }
        else // Use the robot position
        {
          projectToSafety(copied_obstacles, k, pos); // Ensure that the vehicle position is collision-free
          /** @todo Set projected disc position */
        }

        // For all obstacles
        for (size_t obs_id = 0; obs_id < copied_obstacles.size(); obs_id++)
        {
          const auto &copied_obstacle = copied_obstacles[obs_id];
          const Eigen::Vector2d &obstacle_pos = copied_obstacle.prediction.modes[0][k - 1].position;

          double diff_x = obstacle_pos(0) - pos(0);
          double diff_y = obstacle_pos(1) - pos(1);

          double dist = (obstacle_pos - pos).norm();

          // Compute the components of A for this obstacle (normalized normal vector)
          _a1[d][k](obs_id) = diff_x / dist;
          _a2[d][k](obs_id) = diff_y / dist;

          // Compute b (evaluate point on the collision circle)
          double radius = _use_guidance ? 1e-3 : copied_obstacle.radius;

          _b[d][k](obs_id) = _a1[d][k](obs_id) * obstacle_pos(0) +
                             _a2[d][k](obs_id) * obstacle_pos(1) -
                             (radius + CONFIG["robot_radius"].as<double>());
        }

        if (!module_data.static_obstacles.empty() && (int)module_data.static_obstacles[k].size() < _n_other_halfspaces)
        {
          LOG_WARN(_n_other_halfspaces << " halfspaces expected, but "
                                       << (int)module_data.static_obstacles[k].size() << " are present");
        }

        if (!module_data.static_obstacles.empty())
        {

          int num_halfspaces = std::min((int)module_data.static_obstacles[k].size(), _n_other_halfspaces);
          for (int h = 0; h < num_halfspaces; h++)
          {
            int obs_id = copied_obstacles.size() + h;
            _a1[d][k](obs_id) = module_data.static_obstacles[k][h].A(0);
            _a2[d][k](obs_id) = module_data.static_obstacles[k][h].A(1);
            _b[d][k](obs_id) = module_data.static_obstacles[k][h].b;
          }
        }
      }
    }
    LOG_MARK("LinearizedConstraints::update done");
  }

  void LinearizedConstraints::projectToSafety(const std::vector<DynamicObstacle> &copied_obstacles, int k, Eigen::Vector2d &pos)
  {
    if (copied_obstacles.empty()) // There is no anchor
      return;

    // Project to a collision free position if necessary, considering all the obstacles
    for (int iterate = 0; iterate < 3; iterate++) // At most 3 iterations
    {
      for (auto &obstacle : copied_obstacles)
      {
        double radius = _use_guidance ? 1e-3 : obstacle.radius;

        dr_projection_.douglasRachfordProjection(pos, obstacle.prediction.modes[0][k - 1].position,
                                                 copied_obstacles[0].prediction.modes[0][k - 1].position,
                                                 radius + CONFIG["robot_radius"].as<double>(),
                                                 pos);
      }
    }
  }

  void LinearizedConstraints::setParameters(const RealTimeData &data, const ModuleData &module_data, int k)
  {
    (void)module_data;
    int constraint_counter = 0; // Necessary for now to map the disc and obstacle index to a single index

    if (k == 0)
    {
      for (int i = 0; i < _max_obstacles + _n_other_halfspaces; i++)
      {

        setSolverParameterLinConstraintA1(0, _solver->_params, _dummy_a1, constraint_counter);
        setSolverParameterLinConstraintA2(0, _solver->_params, _dummy_a2, constraint_counter);
        setSolverParameterLinConstraintB(0, _solver->_params, _dummy_b, constraint_counter);
        constraint_counter++;
      }
      return;
    }

    for (int d = 0; d < _n_discs; d++)
    {
      if (!_use_guidance)
        setSolverParameterEgoDiscOffset(k, _solver->_params, data.robot_area[d].offset, d);

      for (size_t i = 0; i < data.dynamic_obstacles.size() + _n_other_halfspaces; i++)
      {
        setSolverParameterLinConstraintA1(k, _solver->_params, _a1[d][k](i), constraint_counter);
        setSolverParameterLinConstraintA2(k, _solver->_params, _a2[d][k](i), constraint_counter);
        setSolverParameterLinConstraintB(k, _solver->_params, _b[d][k](i), constraint_counter);
        constraint_counter++;
      }

      for (int i = data.dynamic_obstacles.size() + _n_other_halfspaces; i < _max_obstacles + _n_other_halfspaces; i++)
      {
        setSolverParameterLinConstraintA1(k, _solver->_params, _dummy_a1, constraint_counter);
        setSolverParameterLinConstraintA2(k, _solver->_params, _dummy_a2, constraint_counter);
        setSolverParameterLinConstraintB(k, _solver->_params, _dummy_b, constraint_counter);
        constraint_counter++;
      }
    }
  }

  bool LinearizedConstraints::isDataReady(const RealTimeData &data, std::string &missing_data)
  {
    if ((int)data.dynamic_obstacles.size() != _max_obstacles)
    {
      missing_data += "Obstacles ";
      return false;
    }

    for (size_t i = 0; i < data.dynamic_obstacles.size(); i++)
    {
      if (data.dynamic_obstacles[i].prediction.empty())
      {
        missing_data += "Obstacle Prediction ";
        return false;
      }

      if (data.dynamic_obstacles[i].prediction.type != PredictionType::DETERMINISTIC && data.dynamic_obstacles[i].prediction.type != PredictionType::GAUSSIAN)
      {

        missing_data += "Obstacle Prediction (type must be deterministic, or gaussian) ";
        return false;
      }
    }

    return true;
  }

  void LinearizedConstraints::visualize(const RealTimeData &data, const ModuleData &module_data)
  {
    (void)module_data;
    if (_use_guidance && !CONFIG["debug_visuals"].as<bool>())
      return;

    PROFILE_FUNCTION();

    for (int k = 1; k < _solver->N; k++)
    {
      for (size_t i = 0; i < data.dynamic_obstacles.size(); i++)
      {
        visualizeLinearConstraint(_a1[0][k](i), _a2[0][k](i), _b[0][k](i), k, _solver->N, _name,
                                  k == _solver->N - 1 && i == data.dynamic_obstacles.size() - 1); // Publish at the end
      }
    }
  }

} // namespace MPCPlanner

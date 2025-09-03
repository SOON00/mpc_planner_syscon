#include "mpc_planner_modules/gaussian_constraints.h"

#include <mpc_planner_solver/mpc_planner_parameters.h>

#include <mpc_planner_util/parameters.h>

#include <ros_tools/visuals.h>
#include <ros_tools/math.h>
#include <ros_tools/profiling.h>

#include <algorithm>

namespace MPCPlanner
{
  GaussianConstraints::GaussianConstraints(std::shared_ptr<Solver> solver)
      : ControllerModule(ModuleType::CONSTRAINT, solver, "gaussian_constraints")
  {
    LOG_INITIALIZE("Gaussian Constraints");
    LOG_INITIALIZED();
  }

  void GaussianConstraints::update(State &state, const RealTimeData &data, ModuleData &module_data)
  {
    (void)state;
    (void)data;
    (void)module_data;
    _dummy_x = state.get("x") + 100.;
    _dummy_y = state.get("y") + 100.;
  }

  void GaussianConstraints::setParameters(const RealTimeData &data, const ModuleData &module_data, int k)
  {
    (void)module_data;

    setSolverParameterEgoDiscRadius(k, _solver->_params, CONFIG["robot_radius"].as<double>());
    for (int d = 0; d < CONFIG["n_discs"].as<int>(); d++)
      setSolverParameterEgoDiscOffset(k, _solver->_params, data.robot_area[d].offset, d);

    if (k == 0) // Dummies
    {

      for (size_t i = 0; i < data.dynamic_obstacles.size(); i++)
      {
        setSolverParameterGaussianObstX(k, _solver->_params, _dummy_x, i);
        setSolverParameterGaussianObstY(k, _solver->_params, _dummy_y, i);
        setSolverParameterGaussianObstMajor(k, _solver->_params, 0.1, i);
        setSolverParameterGaussianObstMinor(k, _solver->_params, 0.1, i);
        setSolverParameterGaussianObstRisk(k, _solver->_params, 0.05, i);
        setSolverParameterGaussianObstR(k, _solver->_params, 0.1, i);
      }
      return;
    }

    std::vector<DynamicObstacle> copied_obstacles = data.dynamic_obstacles;

    for (size_t i = 0; i < copied_obstacles.size(); i++)
    {
      const auto &obstacle = copied_obstacles[i];

      if (obstacle.prediction.type == PredictionType::GAUSSIAN)
      {
        setSolverParameterGaussianObstX(k, _solver->_params, obstacle.prediction.modes[0][k - 1].position(0), i);
        setSolverParameterGaussianObstY(k, _solver->_params, obstacle.prediction.modes[0][k - 1].position(1), i);

        if (obstacle.type == ObstacleType::DYNAMIC)
        {
          setSolverParameterGaussianObstMajor(k, _solver->_params, obstacle.prediction.modes[0][k - 1].major_radius, i);
          setSolverParameterGaussianObstMinor(k, _solver->_params, obstacle.prediction.modes[0][k - 1].minor_radius, i);
        }
        else // Static obstacles have no uncertainty
        {
          setSolverParameterGaussianObstMajor(k, _solver->_params, 0.001, i);
          setSolverParameterGaussianObstMinor(k, _solver->_params, 0.001, i);
        }
        setSolverParameterGaussianObstRisk(k, _solver->_params, CONFIG["probabilistic"]["risk"].as<double>(), i);
        setSolverParameterGaussianObstR(k, _solver->_params, CONFIG["obstacle_radius"].as<double>(), i);
      }
    }
  }

  bool GaussianConstraints::isDataReady(const RealTimeData &data, std::string &missing_data)
  {
    if (data.dynamic_obstacles.size() != CONFIG["max_obstacles"].as<unsigned int>())
    {
      missing_data += "Obstacles ";
      return false;
    }

    for (size_t i = 0; i < data.dynamic_obstacles.size(); i++)
    {
      if (data.dynamic_obstacles[i].prediction.modes.empty())
      {
        missing_data += "Obstacle Prediction ";
        return false;
      }

      if (data.dynamic_obstacles[i].prediction.type != PredictionType::GAUSSIAN)
      {
        missing_data += "Obstacle Prediction (Type is not Gaussian) ";
        return false;
      }
    }

    return true;
  }

  void GaussianConstraints::visualize(const RealTimeData &data, const ModuleData &module_data)
  {
    (void)module_data;
    PROFILE_SCOPE("GuidanceConstraints::Visualize");
    LOG_MARK("GaussianConstraints::visualize");
    auto &publisher = VISUALS.getPublisher(_name);

    auto &ellipsoid = publisher.getNewPointMarker("CYLINDER");

    for (auto &obstacle : data.dynamic_obstacles)
    {

      for (int k = 1; k < _solver->N; k += CONFIG["visualization"]["draw_every"].as<int>())
      {
        ellipsoid.setColorInt(k, _solver->N, 0.5);

        double chi = obstacle.type == ObstacleType::DYNAMIC
                         ? RosTools::ExponentialQuantile(0.5, 1.0 - CONFIG["probabilistic"]["risk"].as<double>())
                         : 0.;
        ellipsoid.setScale(2 * (obstacle.prediction.modes[0][k - 1].major_radius * std::sqrt(chi) + obstacle.radius),
                           2 * (obstacle.prediction.modes[0][k - 1].major_radius * std::sqrt(chi) + obstacle.radius), 0.005);

        ellipsoid.addPointMarker(obstacle.prediction.modes[0][k - 1].position);
      }
    }
    publisher.publish();
  }

} // namespace MPCPlanner

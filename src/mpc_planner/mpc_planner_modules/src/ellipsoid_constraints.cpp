#include "mpc_planner_modules/ellipsoid_constraints.h"

#include <mpc_planner_solver/mpc_planner_parameters.h>

#include <mpc_planner_util/parameters.h>

#include <ros_tools/visuals.h>
#include <ros_tools/math.h>
#include <algorithm>

namespace MPCPlanner
{
  EllipsoidConstraints::EllipsoidConstraints(std::shared_ptr<Solver> solver)
      : ControllerModule(ModuleType::CONSTRAINT, solver, "ellipsoid_constraints")
  {
    LOG_INITIALIZE("Ellipsoid Constraints");
    LOG_INITIALIZED();

    _n_discs = CONFIG["n_discs"].as<int>();
    _robot_radius = CONFIG["robot_radius"].as<double>();
    _risk = CONFIG["probabilistic"]["risk"].as<double>();
  }

  void EllipsoidConstraints::update(State &state, const RealTimeData &data, ModuleData &module_data)
  {
    (void)state;
    (void)data;
    (void)module_data;

    _dummy_x = state.get("x") + 50;
    _dummy_y = state.get("y") + 50;
  }

  void EllipsoidConstraints::setParameters(const RealTimeData &data, const ModuleData &module_data, int k)
  {
    (void)module_data;

    setSolverParameterEgoDiscRadius(k, _solver->_params, _robot_radius);
    for (int d = 0; d < _n_discs; d++)
      setSolverParameterEgoDiscOffset(k, _solver->_params, data.robot_area[d].offset, d);

    if (k == 0) // Dummies
    {
      // LOG_INFO("Setting parameters for k = 0");
      for (size_t i = 0; i < data.dynamic_obstacles.size(); i++)
      {
        setSolverParameterEllipsoidObstX(0, _solver->_params, _dummy_x, i);
        setSolverParameterEllipsoidObstY(0, _solver->_params, _dummy_y, i);
        setSolverParameterEllipsoidObstPsi(0, _solver->_params, 0., i);
        setSolverParameterEllipsoidObstR(0, _solver->_params, 0.1, i);
        setSolverParameterEllipsoidObstMajor(0, _solver->_params, 0., i);
        setSolverParameterEllipsoidObstMinor(0, _solver->_params, 0., i);
        setSolverParameterEllipsoidObstChi(0, _solver->_params, 1., i);
      }
      return;
    }

    if (k == 1)
      LOG_MARK("EllipsoidConstraints::setParameters");

    for (size_t i = 0; i < data.dynamic_obstacles.size(); i++)
    {
      const auto &obstacle = data.dynamic_obstacles[i];
      const auto &mode = obstacle.prediction.modes[0];

      /** @note The first prediction step is index 1 of the optimization problem, i.e., k-1 maps to the predictions for this stage */
      setSolverParameterEllipsoidObstX(k, _solver->_params, mode[k - 1].position(0), i);
      setSolverParameterEllipsoidObstY(k, _solver->_params, mode[k - 1].position(1), i);
      setSolverParameterEllipsoidObstPsi(k, _solver->_params, mode[k - 1].angle, i);
      setSolverParameterEllipsoidObstR(k, _solver->_params, obstacle.radius, i);

      if (obstacle.prediction.type == PredictionType::DETERMINISTIC)
      {
        setSolverParameterEllipsoidObstMajor(k, _solver->_params, 0., i);
        setSolverParameterEllipsoidObstMinor(k, _solver->_params, 0., i);
        setSolverParameterEllipsoidObstChi(k, _solver->_params, 1., i);
      }
      else if (obstacle.prediction.type == PredictionType::GAUSSIAN)
      {
        double chi = RosTools::ExponentialQuantile(0.5, 1.0 - _risk);

        setSolverParameterEllipsoidObstMajor(k, _solver->_params, mode[k - 1].major_radius, i);
        setSolverParameterEllipsoidObstMinor(k, _solver->_params, mode[k - 1].minor_radius, i);
        setSolverParameterEllipsoidObstChi(k, _solver->_params, chi, i);
      }
    }

    if (k == 1)
      LOG_MARK("EllipsoidConstraints::setParameters Done");
  }

  bool EllipsoidConstraints::isDataReady(const RealTimeData &data, std::string &missing_data)
  {
    if (data.robot_area.size() == 0)
    {
      missing_data += "Robot area ";
      return false;
    }

    if (data.dynamic_obstacles.size() != CONFIG["max_obstacles"].as<unsigned int>())
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

      if (data.dynamic_obstacles[i].prediction.type != PredictionType::GAUSSIAN && data.dynamic_obstacles[i].prediction.type != PredictionType::DETERMINISTIC)
      {
        missing_data += "Obstacle Prediction (Type is incorrect) ";
        return false;
      }
    }

    return true;
  }

  void EllipsoidConstraints::visualize(const RealTimeData &data, const ModuleData &module_data)
  {
    (void)data;
    (void)module_data;
    //   if (_spline.get() == nullptr)
    //     return;

    //   // Visualize the current points
    //   auto &publisher_current = VISUALS.getPublisher(_name + "/current");
    //   auto &cur_point = publisher_current.getNewPointMarker("CUBE");
    //   cur_point.setColorInt(10);
    //   cur_point.setScale(0.3, 0.3, 0.3);
    //   cur_point.addPointMarker(_spline->getPoint(_spline->getStartOfSegment(_closest_segment)), 0.0);
    //   publisher_current.publish();

    //   // Visualize the points
    //   auto &publisher_points = VISUALS.getPublisher(_name + "/points");
    //   auto &point = publisher_points.getNewPointMarker("CYLINDER");
    //   point.setColor(0., 0., 0.);
    //   point.setScale(0.15, 0.15, 0.05);

    //   for (size_t p = 0; p < data.reference_path.x.size(); p++)
    //     point.addPointMarker(Eigen::Vector3d(data.reference_path.x[p], data.reference_path.y[p], 0.1));
    //   publisher_points.publish();

    //   // Visualize the path
    //   auto &publisher_path = VISUALS.getPublisher(_name + "/path");
    //   auto &line = publisher_path.getNewLine();
    //   line.setColorInt(5);
    //   line.setScale(0.1);

    //   Eigen::Vector2d p;
    //   for (double s = 0.; s < _spline->length(); s += 1.)
    //   {
    //     if (s > 0.)
    //       line.addLine(p, _spline->getPoint(s));

    //     p = _spline->getPoint(s);
    //   }

    //   publisher_path.publish();
  }

} // namespace MPCPlanner

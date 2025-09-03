#include "mpc_planner_types/data_types.h"

/** Basic high-level data types for motion planning */

namespace MPCPlanner
{

    Disc::Disc(const double offset_, const double radius_)
        : offset(offset_), radius(radius_)
    {
    }

    Eigen::Vector2d Disc::getPosition(const Eigen::Vector2d &robot_position, const double angle) const
    {
        return robot_position + Eigen::Vector2d(offset * std::cos(angle), offset * std::sin(angle));
    }

    Eigen::Vector2d Disc::toRobotCenter(const Eigen::Vector2d &disc_position, const double angle) const
    {
        return disc_position - Eigen::Vector2d(offset * std::cos(angle), offset * std::sin(angle));
    }

    Halfspace::Halfspace(const Eigen::Vector2d &A, const double b)
        : A(A), b(b)
    {
    }

    PredictionStep::PredictionStep(const Eigen::Vector2d &position, double angle, double major_radius, double minor_radius)
        : position(position), angle(angle), major_radius(major_radius), minor_radius(minor_radius)
    {
    }

    Prediction::Prediction()
        : type(PredictionType::NONE)
    {
    }

    Prediction::Prediction(PredictionType type)
        : type(type)
    {
        if (type == PredictionType::DETERMINISTIC || type == PredictionType::GAUSSIAN)
        {
            modes.emplace_back();
            probabilities.emplace_back(1.);
        }
    }

    bool Prediction::empty() const
    {
        return modes.empty() || (modes.size() > 0 && modes[0].empty());
    }

    DynamicObstacle::DynamicObstacle(int _index, const Eigen::Vector2d &_position, double _angle, double _radius, ObstacleType _type)
        : index(_index), position(_position), angle(_angle), radius(_radius)
    {
        type = _type;
    }

    ReferencePath::ReferencePath(int length)
    {
        x.reserve(length);
        y.reserve(length);
        psi.reserve(length);
        v.reserve(length);
        s.reserve(length);
    }

    void ReferencePath::clear()
    {
        x.clear();
        y.clear();
        psi.clear();
        v.clear();
        s.clear();
    }

    bool ReferencePath::pointInPath(int point_num, double other_x, double other_y) const
    {
        return (x[point_num] == other_x && y[point_num] == other_y);
    }

    Trajectory::Trajectory(double dt, int length) : dt(dt)
    {
        positions.reserve(length);
    }

    void Trajectory::add(const Eigen::Vector2d &p)
    {
        positions.push_back(p);
    }

    void Trajectory::add(const double x, const double y)
    {
        positions.push_back(Eigen::Vector2d(x, y));
    }

    FixedSizeTrajectory::FixedSizeTrajectory(int size)
        : _size(size)
    {
        positions.reserve(size);
    }

    void FixedSizeTrajectory::add(const Eigen::Vector2d &p)
    {
        // On jump, erase the trajectory
        if (std::sqrt((p - positions.back()).transpose() * (p - positions.back())) > 5.0)
        {
            positions.clear();
            positions.push_back(p);
            return;
        }

        if ((int)positions.size() < _size)
        {
            positions.push_back(p);
        }
        else
        {
            positions.erase(positions.begin());
            positions.push_back(p);
        }
    }
}

#ifndef DATA_VISUALIZATION_H
#define DATA_VISUALIZATION_H

#include <Eigen/Dense>
#include <string>
#include <vector>

namespace RosTools
{
    class ROSMarkerPublisher;
    class Spline2D;
}
// Tools for visualizing custom data types
namespace MPCPlanner
{
    struct Trajectory;
    struct DynamicObstacle;
    struct Disc;
    struct Halfspace;
    struct ReferencePath;

    RosTools::ROSMarkerPublisher &visualizePathPoints(const ReferencePath &path, const std::string &topic_name,
                                                      bool publish = false, double alpha = 1.0);

    RosTools::ROSMarkerPublisher &visualizeSpline(const RosTools::Spline2D &path, const std::string &topic_name,
                                                  bool publish = false, double alpha = 1.0,
                                                  int color_index = 5, int color_max = 10);

    RosTools::ROSMarkerPublisher &visualizeTrajectory(const Trajectory &trajectory, const std::string &topic_name,
                                                      bool publish = false, double alpha = 0.4,
                                                      int color_index = 0, int color_max = 10,
                                                      bool publish_trace = true, bool publish_regions = true);

    RosTools::ROSMarkerPublisher &visualizeObstacles(const std::vector<DynamicObstacle> &obstacles,
                                                     const std::string &topic_name,
                                                     bool publish = false, double alpha = 0.6);

    RosTools::ROSMarkerPublisher &visualizeObstaclePredictions(const std::vector<DynamicObstacle> &obstacles,
                                                               const std::string &topic_name,
                                                               bool publish = false, double alpha = 0.3);

    RosTools::ROSMarkerPublisher &visualizeLinearConstraint(double a1, double a2, double b, int k, int N,
                                                            const std::string &topic_name,
                                                            bool publish = false, double alpha = 1.0, double thickness = 0.1);

    RosTools::ROSMarkerPublisher &visualizeLinearConstraint(const Halfspace &halfspace, int k, int N,
                                                            const std::string &topic_name,
                                                            bool publish = false, double alpha = 1.0, double thickness = 0.1);

    RosTools::ROSMarkerPublisher &visualizeRobotArea(const Eigen::Vector2d &position, const double angle,
                                                     const std::vector<Disc> robot_area,
                                                     const std::string &topic_name,
                                                     bool publish = false, double alpha = 1.0);

    RosTools::ROSMarkerPublisher &visualizeRectangularRobotArea(const Eigen::Vector2d &position, const double angle,
                                                                const double width, const double length,
                                                                const std::string &topic_name,
                                                                bool publish = false, double alpha = 1.0);

    RosTools::ROSMarkerPublisher &visualizeRobotAreaTrajectory(const Trajectory &trajectory,
                                                               const std::vector<double> angles,
                                                               const std::vector<Disc> robot_area,
                                                               const std::string &topic_name,
                                                               bool publish = false, double alpha = 1.0);
}

#endif // DATA_VISUALIZATION_H
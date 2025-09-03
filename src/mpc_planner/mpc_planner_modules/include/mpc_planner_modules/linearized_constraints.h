#ifndef __LINEARIZED_CONSTRAINTS_H_
#define __LINEARIZED_CONSTRAINTS_H_

#include <mpc_planner_modules/controller_module.h>

#include <ros_tools/projection.h>

namespace MPCPlanner
{
  class LinearizedConstraints : public ControllerModule
  {
  public:
    LinearizedConstraints(std::shared_ptr<Solver> solver);

  public:
    void update(State &state, const RealTimeData &data, ModuleData &module_data) override;
    void setParameters(const RealTimeData &data, const ModuleData &module_data, int k) override;

    bool isDataReady(const RealTimeData &data, std::string &missing_data) override;

    void visualize(const RealTimeData &data, const ModuleData &module_data) override;

    void setTopologyConstraints();

  private:
    std::vector<std::vector<Eigen::ArrayXd>> _a1, _a2, _b; // Constraints [disc x step]

    double _dummy_a1{1.}, _dummy_a2{0.}, _dummy_b;

    bool _use_guidance{false};
    int _n_discs;
    int _n_other_halfspaces;

    DouglasRachford dr_projection_;

    int _num_obstacles, _max_obstacles;

    void projectToSafety(const std::vector<DynamicObstacle> &copied_obstacles, int k, Eigen::Vector2d &pos);
  };
} // namespace MPCPlanner
#endif // __LINEARIZED_CONSTRAINTS_H_

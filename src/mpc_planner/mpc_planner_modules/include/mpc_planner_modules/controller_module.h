#ifndef __CONTROLLER_MODULE_H__
#define __CONTROLLER_MODULE_H__

#include <mpc_planner_types/module_data.h>
#include <mpc_planner_types/realtime_data.h>
#include <mpc_planner_solver/solver_interface.h>

#include <ros_tools/logging.h>

#include <memory>

// To distinguish custom from regular optimization loops.
#define EXIT_CODE_NOT_OPTIMIZED_YET -999

namespace RosTools
{
    class DataSaver;
}

namespace MPCPlanner
{

    enum class ModuleType
    {
        OBJECTIVE = 0,
        CONSTRAINT,
        UNDEFINED
    };

    /**
     * @brief Purely virtual module of the controller to compute inequalities, objective terms or anything else.
     * The idea is that modules are defined in the solver and seemingly integrate with the c++ code without having to adapt parameters on either side.
     * This should make the process of stacking different MPC contributions more flexible.
     */
    class ControllerModule
    {
    public:
        /**
         * @brief Construct a new Controller Module object. Note that controller module initialization happens in the solver class itself based on the
         * python code.
         */
        ControllerModule(ModuleType module_type, std::shared_ptr<Solver> solver, const std::string &&module_name)
            : type(module_type), _solver(solver), _name(module_name)
        {
        }

        virtual ~ControllerModule(){};

    public:
        /** ==== MAIN FUNCTIONS ==== */
        /** @brief Update the module (any computations that need to happen before setting solver parameters) */
        virtual void update(State &state, const RealTimeData &data, ModuleData &module_data)
        {
            (void)state;
            (void)data;
            (void)module_data;
        };

        /** @brief Insert computed parameters for the solver */
        virtual void setParameters(const RealTimeData &data, const ModuleData &module_data, int k)
        {
            (void)data;
            (void)module_data;
            (void)k;
        };

        /** @brief Visualize the computations in this module */
        virtual void visualize(const RealTimeData &data, const ModuleData &module_data)
        {
            (void)data;
            (void)module_data;
        };

        /** ==== OPTIONAL FUNCTIONS ==== */
        /** @brief Check if the realtime data is complete for this module */
        virtual bool isDataReady(const RealTimeData &data, std::string &missing_data)
        {
            (void)data;
            (void)missing_data;
            return true;
        };

        /**
         * @brief Check if the objective of this module was reached
         * @return true If the objective was reached
         */
        virtual bool isObjectiveReached(const State &state, const RealTimeData &data)
        {
            (void)state;
            (void)data;
            return true;
        }; // Default: true

        /**
         * @brief Function used to update any class members when new data is received
         * @param data_name The name of the data that was updated (to decide if anything needs to be updated)
         */
        virtual void onDataReceived(RealTimeData &data, std::string &&data_name)
        {
            (void)data;
            (void)data_name;
        };
        virtual void reset(){};

        /**
         * @brief Override to define a custom optimization loop. Note that there can only be ONE customized optimization.
         * @return int exit_code of the solver, return any exit_code other than "EXIT_CODE_NOT_OPTIMIZED_YET" to define this as a custom optimization
         */
        virtual int optimize(State &state, const RealTimeData &data, ModuleData &module_data)
        {
            (void)state;
            (void)data;
            (void)module_data;
            return EXIT_CODE_NOT_OPTIMIZED_YET;
        }; // Default: no custom optimization

        /** @todo: Add reconfigurable parameters! */

        /** @brief Export runtime data */
        virtual void saveData(RosTools::DataSaver &data_saver){(void)data_saver;};

        /**
         * @brief Assign a name for this controller
         *
         * @param name A name reference. Names can be added or overwritten
         */
        // virtual void GetMethodName(std::string &name){};

        /** ================================== */

    public:
        ModuleType type; /* Constraint or Objective type */

    protected:
        std::shared_ptr<Solver> _solver;
        std::string _name;
    };
}
#endif
#!/usr/bin/python3

import os
import sys

# TODO: Import packages through pypi
sys.path.append(os.path.join(sys.path[0], "..", "..", "solver_generator"))
sys.path.append(os.path.join(sys.path[0], "..", "..", "mpc_planner_modules", "scripts"))

# SET YOUR FORCES PATH HERE (can also be in PYTHONPATH)
forces_path = os.path.join(os.path.expanduser("~"), "forces_pro_client")
sys.path.append(forces_path)

from util.files import load_settings, get_current_package
from control_modules import ModuleManager
from generate_solver import generate_solver

# Import modules here from mpc_planner_modules
from mpc_base import MPCBaseModule
from contouring import ContouringModule
from path_reference_velocity import PathReferenceVelocityModule

from ellipsoid_constraints import EllipsoidConstraintModule
from gaussian_constraints import GaussianConstraintModule
from decomp_constraints import DecompConstraintModule
from guidance_constraints import GuidanceConstraintModule
from linearized_constraints import LinearizedConstraintModule
from scenario_constraints import ScenarioConstraintModule

# Import solver models that you want to use
from solver_model import ContouringSecondOrderUnicycleModel, ContouringSecondOrderUnicycleModelWithSlack


def define_modules(settings) -> ModuleManager:
    modules = ModuleManager()

    # Module that allows for penalization of variables
    base_module = modules.add_module(MPCBaseModule(settings))

    # Penalize ||a||_2^2 and ||w||_2^2
    base_module.weigh_variable(var_name="a", weight_names="acceleration")
    base_module.weigh_variable(var_name="w", weight_names="angular_velocity")
    base_module.weigh_variable(var_name="slack", weight_names="slack")

    # Penalize ||v - v_ref||_2^2
    # base_module.weigh_variable(
    #     var_name="v",
    #     weight_names=["velocity", "reference_velocity"],
    #     cost_function=lambda x, w: w[0] * (x - w[1]) ** 2,
    # )

    # modules.add_module(GoalModule(settings))  # Track a goal
    modules.add_module(ContouringModule(settings))
    modules.add_module(PathReferenceVelocityModule(settings))

    modules.add_module(EllipsoidConstraintModule(settings))
    # modules.add_module(ScenarioConstraintModule(settings))
    # modules.add_module(GuidanceConstraintModule(settings, constraint_submodule=EllipsoidConstraintModule))
    # modules.add_module(LinearizedConstraintModule(settings))

    return modules


def configuration_safe_horizon(settings):
    modules = ModuleManager()
    model = ContouringSecondOrderUnicycleModelWithSlack()

    # Module that allows for penalization of variables
    base_module = modules.add_module(MPCBaseModule(settings))

    # Penalize ||a||_2^2 and ||w||_2^2
    base_module.weigh_variable(var_name="a", weight_names="acceleration")
    base_module.weigh_variable(var_name="w", weight_names="angular_velocity")
    base_module.weigh_variable(var_name="slack", weight_names="slack", rqt_max_value=10000.0)

    # modules.add_module(GoalModule(settings))  # Track a goal
    if not settings["contouring"]["dynamic_velocity_reference"]:
        base_module.weigh_variable(var_name="v",    
                                weight_names=["velocity", "reference_velocity"], 
                                cost_function=lambda x, w: w[0] * (x-w[1])**2)

    modules.add_module(ContouringModule(settings))
    if settings["contouring"]["dynamic_velocity_reference"]:
        modules.add_module(PathReferenceVelocityModule(settings))

    modules.add_module(ScenarioConstraintModule(settings))
    modules.add_module(DecompConstraintModule(settings))
    return model, modules


def configuration_tmpc(settings):
    modules = ModuleManager()
    model = ContouringSecondOrderUnicycleModelWithSlack()

    base_module = modules.add_module(MPCBaseModule(settings))
    base_module.weigh_variable(var_name="a", weight_names="acceleration")
    base_module.weigh_variable(var_name="w", weight_names="angular_velocity")
    base_module.weigh_variable(var_name="slack", weight_names="slack")
    if not settings["contouring"]["dynamic_velocity_reference"]:
        base_module.weigh_variable(var_name="v",    
                                weight_names=["velocity", "reference_velocity"], 
                                cost_function=lambda x, w: w[0] * (x-w[1])**2)

    modules.add_module(ContouringModule(settings))
    if settings["contouring"]["dynamic_velocity_reference"]:
        modules.add_module(PathReferenceVelocityModule(settings))

    # modules.add_module(GuidanceConstraintModule(settings, constraint_submodule=GaussianConstraintModule))
    modules.add_module(GuidanceConstraintModule(settings, constraint_submodule=EllipsoidConstraintModule))
    modules.add_module(DecompConstraintModule(settings))

    return model, modules


def configuration_lmpcc(settings):
    modules = ModuleManager()
    model = ContouringSecondOrderUnicycleModel()

    # Penalize ||a||_2^2 and ||w||_2^2
    base_module = modules.add_module(MPCBaseModule(settings))
    base_module.weigh_variable(var_name="a", weight_names="acceleration")
    base_module.weigh_variable(var_name="w", weight_names="angular_velocity")

    modules.add_module(ContouringModule(settings))
    modules.add_module(PathReferenceVelocityModule(settings))

    modules.add_module(EllipsoidConstraintModule(settings))
    modules.add_module(DecompConstraintModule(settings))

    return model, modules


settings = load_settings()

# model, modules = configuration_safe_horizon(settings)
# model, modules = configuration_lmpcc(settings)
model, modules = configuration_tmpc(settings)

generate_solver(modules, model, settings)

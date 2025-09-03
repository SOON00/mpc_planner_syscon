import os, shutil

import numpy as np
import casadi as cd

from acados_template import AcadosModel
from acados_template import AcadosOcp, AcadosOcpSolver, AcadosSimSolver

from util.files import solver_name, solver_path, default_solver_path, default_acados_solver_path, acados_solver_path
from util.logging import print_value, print_success, print_header, print_warning, print_path
from util.parameters import Parameters, AcadosParameters

from solver_definition import define_parameters, objective, constraints, constraint_lower_bounds, constraint_upper_bounds
import solver_model


def parse_constraint_bounds(bounds):
    large_value = 1e15  # Acados does not support inf
    for i in range(len(bounds)):
        if bounds[i] == np.inf:
            bounds[i] = large_value
        if bounds[i] == -np.inf:
            bounds[i] = -large_value
    return np.array(bounds)


def create_acados_model(settings, model, modules):
    # Create an acados ocp model
    acados_model = AcadosModel()
    acados_model.name = solver_name(settings)

    # Dynamics
    z = model.acados_symbolics()
    dyn_f_expl, dyn_f_impl = model.get_acados_dynamics()

    # Parameters
    params = settings["params"]
    p = params.get_acados_p()

    # Constraints
    constr = cd.vertcat(*constraints(modules, z, p, model, settings, 1))

    if constr.shape[0] == 0:
        print("No constraints specified")
        constr = cd.SX()

    # stage cost
    cost_stage = objective(modules, z, p, model, settings, 1)

    # terminal cost
    cost_e = objective(modules, z, p, model, settings, settings["N"] - 1)

    # Formulating acados ocp model
    acados_model.x = model.get_x()
    acados_model.u = model.get_acados_u()
    acados_model.xdot = model.get_acados_x_dot()
    acados_model.f_expl_expr = dyn_f_expl
    acados_model.f_impl_expr = dyn_f_impl
    acados_model.p = params.get_acados_parameters()

    acados_model.cost_expr_ext_cost = cost_stage
    acados_model.cost_expr_ext_cost_e = cost_e
    acados_model.con_h_expr = constr

    return acados_model


def generate_acados_solver(modules, settings, model, skip_solver_generation):

    params = AcadosParameters()
    define_parameters(modules, params, settings)
    params.load_acados_parameters()
    settings["params"] = params
    solver_settings = settings["solver_settings"]

    modules.print()
    params.print()

    npar = params.length()

    model_acados = create_acados_model(settings, model, modules)

    # Create an acados ocp object
    ocp = AcadosOcp()
    ocp.model = model_acados

    # Set ocp dimensions
    ocp.dims.N = settings["N"]

    # Set cost types
    ocp.cost.cost_type = "EXTERNAL"
    # ocp.cost.cost_type_e = "EXTERNAL"

    # Set initial constraint
    ocp.constraints.x0 = np.zeros(model.nx)

    # Set state bound
    nx = model.nx
    nu = model.nu
    ocp.constraints.lbx = np.array([model.lower_bound[nu : model.get_nvar()]]).flatten()
    ocp.constraints.ubx = np.array([model.upper_bound[nu : model.get_nvar()]]).flatten()
    ocp.constraints.idxbx = np.array(range(model.nx))

    # Set control input bound
    ocp.constraints.lbu = np.array([model.lower_bound[:nu]]).flatten()
    ocp.constraints.ubu = np.array([model.upper_bound[:nu]]).flatten()
    ocp.constraints.idxbu = np.array(range(nu))

    # Set path constraints bound
    nc = ocp.model.con_h_expr.shape[0]
    ocp.constraints.lh = parse_constraint_bounds(constraint_lower_bounds(modules))
    ocp.constraints.uh = parse_constraint_bounds(constraint_upper_bounds(modules))

    ocp.parameter_values = np.zeros(model_acados.p.size()[0])

    # horizon
    ocp.solver_options.tf = settings["N"] * settings["integrator_step"]
    ocp.solver_options.tol = 1e-2

    # Solver options
    # integrator option
    ocp.solver_options.integrator_type = "ERK"
    ocp.solver_options.sim_method_num_stages = 4
    ocp.solver_options.sim_method_num_steps = 3  # Number of divisions over the time horizon (ERK applied on each)

    # nlp solver options
    ocp.solver_options.nlp_solver_type = settings["solver_settings"]["acados"]["solver_type"]
    ocp.solver_options.hessian_approx = "EXACT"
    ocp.solver_options.regularize_method = "MIRROR" # Necessary to converge
    ocp.solver_options.globalization = "FIXED_STEP"
    ocp.solver_options.qp_tol = 1e-5  # Important! (1e-3)

    # qp solver options
    ocp.solver_options.qp_solver = "PARTIAL_CONDENSING_HPIPM"
    ocp.solver_options.qp_solver_iter_max = 50  # default = 50
    ocp.solver_options.qp_solver_warm_start = 2  # cold start / 1 = warm, 2 = warm primal and dual

    # code generation options
    ocp.code_export_directory = f"{os.path.dirname(os.path.abspath(__file__))}/acados/{model_acados.name}"
    ocp.solver_options.print_level = 0

    # Generate the solver
    json_file_dir = f"{os.path.dirname(os.path.abspath(__file__))}/acados/{model_acados.name}/"
    json_file_name = json_file_dir + f"{model_acados.name}.json"
    os.makedirs(json_file_dir, exist_ok=True)

    if skip_solver_generation:
        print_header("Output")
        print_warning("Solver generation was disabled by the command line option. Skipped.", no_tab=True)
        return None, None
    else:
        print_header("Generating solver")
        solver = AcadosOcpSolver(acados_ocp=ocp, json_file=json_file_name)

        simulator = AcadosSimSolver(ocp, json_file=json_file_name)
        print_header("Output")

        if os.path.exists(acados_solver_path(settings)) and os.path.isdir(acados_solver_path(settings)):
            shutil.rmtree(acados_solver_path(settings))

        shutil.move(default_acados_solver_path(settings), acados_solver_path(settings))  # Move the solver to this directory

    return solver, simulator


if __name__ == "__main__":
    generate_acados_solver()

import casadi as cd
from spline import Spline, Spline2D
from util.math import haar_difference_without_abs

from parameters import Parameters


# MPCC: Tracks a 2D reference path with contouring costs
class ContouringModule:

    def __init__(self, settings):
        self.type = "objective"
        self.num_segments = settings["contouring"]["num_segments"]
        self.dynamic_velocity_reference = settings["contouring"]["dynamic_velocity_reference"]

    def define_parameters(self, params : Parameters):
        params.add("contour")
        params.add("lag")
        
        if not params.has_parameter("velocity"):
            params.add("velocity")
            params.add("reference_velocity")

        params.add("terminal_angle")
        params.add("terminal_contouring")
        for i in range(self.num_segments):
            params.add(f"spline_x{i}_a")
            params.add(f"spline_x{i}_b")
            params.add(f"spline_x{i}_c")
            params.add(f"spline_x{i}_d")

            params.add(f"spline_y{i}_a")
            params.add(f"spline_y{i}_b")
            params.add(f"spline_y{i}_c")
            params.add(f"spline_y{i}_d")

            params.add(f"spline{i}_start")

        return params

    def get_value(self, model, params: Parameters, settings, stage_idx):
        cost = 0

        pos_x = model.get("x")
        pos_y = model.get("y")
        psi = model.get("psi")
        v = model.get("v")
        s = model.get("spline")

        contour_weight = params.get("contour")
        lag_weight = params.get("lag")

        # From path
        if self.dynamic_velocity_reference:
            if not params.has_parameter("spline_v0_a"):
                raise IOError("contouring/dynamic_velocity_reference is enabled, but there is no PathReferenceVelocity module.")
        
            path_velocity = Spline(params, "spline_v", self.num_segments, s)
            reference_velocity = path_velocity.at(s)
            velocity_weight = params.get("velocity")

        path = Spline2D(params, self.num_segments, s)
        path_x, path_y = path.at(s)
        path_dx_normalized, path_dy_normalized = path.deriv_normalized(s)

        # MPCC
        contour_error = path_dy_normalized * (pos_x - path_x) - path_dx_normalized * (pos_y - path_y)
        lag_error = path_dx_normalized * (pos_x - path_x) + path_dy_normalized * (pos_y - path_y)

        cost += lag_weight * lag_error**2
        cost += contour_weight * contour_error**2

        if self.dynamic_velocity_reference:
            cost += velocity_weight * (v - reference_velocity) ** 2

        # Terminal cost
        if True and stage_idx == settings["N"] - 1:

            terminal_angle_weight = params.get("terminal_angle")
            terminal_contouring_mp = params.get("terminal_contouring")

            # Compute the angle w.r.t. the path
            path_angle = cd.atan2(path_dy_normalized, path_dx_normalized)
            angle_error = haar_difference_without_abs(psi, path_angle)

            # Penalize the angle error
            cost += terminal_angle_weight * angle_error**2
            cost += terminal_contouring_mp * lag_weight * lag_error**2
            cost += terminal_contouring_mp * contour_weight * contour_error**2
           
        return cost
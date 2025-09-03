import os
import sys

import casadi as cd
import numpy as np
import math

from util.math import rotation_matrix, haar_difference_without_abs
from control_modules import ConstraintModule

from spline import Spline2D, Spline

sys.path.append(os.path.join(sys.path[0], "..", "..", "solver_generator"))


class ContouringConstraintModule(ConstraintModule):

    def __init__(self, settings):
        super().__init__()

        self.module_name = "ContouringConstraints"  # Needs to correspond to the c++ name of the module
        self.import_name = "contouring_constraints.h"
        self.description = "Constrain the contouring error to stay within road boundaries"

        self.constraints.append(ContouringConstraint(settings["contouring"]["num_segments"]))


class ContouringConstraint:

    def __init__(self, num_segments):
        self.num_segments = num_segments
        self.nh = 2

    def define_parameters(self, params):
        for i in range(self.num_segments):
                params.add(f"width_right{i}_a", bundle_name="width_right_a")
                params.add(f"width_right{i}_b", bundle_name="width_right_b")
                params.add(f"width_right{i}_c", bundle_name="width_right_c")
                params.add(f"width_right{i}_d", bundle_name="width_right_d")

                params.add(f"width_left{i}_a", bundle_name="width_left_a")
                params.add(f"width_left{i}_b", bundle_name="width_left_b")
                params.add(f"width_left{i}_c", bundle_name="width_left_c")
                params.add(f"width_left{i}_d", bundle_name="width_left_d")

    def get_lower_bound(self):
        lower_bound = []
        lower_bound.append(-np.inf)
        lower_bound.append(-np.inf)
        # lower_bound.append(-np.inf)
        return lower_bound

    def get_upper_bound(self):
        upper_bound = []
        upper_bound.append(0.)
        upper_bound.append(0.)
        # upper_bound.append(0.)
        return upper_bound

    def get_constraints(self, model, params, settings, stage_idx):
        constraints = []
        pos_x = model.get("x")
        pos_y = model.get("y")
        s = model.get("spline")

        try:
            slack = model.get("slack")
        except:
            slack = 0.0

        try:
            psi = model.get("psi")
        except:
            psi = 0.0

        spline = Spline2D(params, self.num_segments, s)
        path_x, path_y = spline.at(s)
        path_dx_normalized, path_dy_normalized = spline.deriv_normalized(s)

        contour_error = path_dy_normalized * (pos_x - path_x) - path_dx_normalized * (pos_y - path_y)

        width_left = Spline(params, "width_left", self.num_segments, s)
        width_right = Spline(params, "width_right", self.num_segments, s)


        # Accurate width of the vehicle incorporating its orientation w.r.t. the path
        # delta_psi = haar_difference_without_abs(psi, cd.atan2(path_dy_normalized, path_dx_normalized)) # Angle w.r.t. the path
        # w_cur = model.width / 2. * cd.cos(delta_psi) + model.lr * cd.sin(cd.fabs(delta_psi))

        # Simpler
        w_cur = model.width / 2. 

        # Forces does not support bounds that depend on the parameters. Two constraints are needed.
        constraints.append(contour_error + w_cur - width_right.at(s) - slack)
        constraints.append(-contour_error + w_cur - width_left.at(s) - slack) # -width_left because widths are positive

        # constraints.append(cd.fabs(delta_psi) - 0.35 * np.pi)

        return constraints

import os
import sys

import casadi as cd
import numpy as np

from util.math import rotation_matrix
from control_modules import ConstraintModule

sys.path.append(os.path.join(sys.path[0], "..", "..", "solver_generator"))


class EllipsoidConstraintModule(ConstraintModule):

    def __init__(self, settings):
        super().__init__()

        self.n_discs = settings["n_discs"]
        self.max_obstacles = settings["max_obstacles"]

        self.module_name = "EllipsoidConstraints"  # Needs to correspond to the c++ name of the module
        self.import_name = "ellipsoid_constraints.h"
        self.description = "Avoid obstacles, modeled as ellipsoids (possibly including Gaussian noise)."

        self.constraints.append(EllipsoidConstraint(self.n_discs, self.max_obstacles))


class EllipsoidConstraint:

    def __init__(self, n_discs, max_obstacles):
        self.max_obstacles = max_obstacles
        self.n_discs = n_discs

        self.nh = max_obstacles * n_discs

    def define_parameters(self, params):
        params.add("ego_disc_radius")

        for disc_id in range(self.n_discs):
            params.add(f"ego_disc_{disc_id}_offset", bundle_name="ego_disc_offset")

        for obs_id in range(self.max_obstacles):
            params.add(f"ellipsoid_obst_{obs_id}_x", bundle_name="ellipsoid_obst_x")
            params.add(f"ellipsoid_obst_{obs_id}_y", bundle_name="ellipsoid_obst_y")
            params.add(f"ellipsoid_obst_{obs_id}_psi", bundle_name="ellipsoid_obst_psi")
            params.add(f"ellipsoid_obst_{obs_id}_major", bundle_name="ellipsoid_obst_major")
            params.add(f"ellipsoid_obst_{obs_id}_minor", bundle_name="ellipsoid_obst_minor")
            params.add(f"ellipsoid_obst_{obs_id}_chi", bundle_name="ellipsoid_obst_chi")
            params.add(f"ellipsoid_obst_{obs_id}_r", bundle_name="ellipsoid_obst_r")

    def get_lower_bound(self):
        lower_bound = []
        for obs in range(self.max_obstacles):
            for disc in range(self.n_discs):
                lower_bound.append(1.0)
        return lower_bound

    def get_upper_bound(self):
        upper_bound = []
        for obs in range(self.max_obstacles):
            for disc in range(self.n_discs):
                upper_bound.append(np.inf)
        return upper_bound

    def get_constraints(self, model, params, settings, stage_idx):
        constraints = []
        pos_x = model.get("x")
        pos_y = model.get("y")
        pos = np.array([pos_x, pos_y])

        try:
            psi = model.get("psi")
        except:
            psi = 0.0

        rotation_car = rotation_matrix(psi)

        r_disc = params.get("ego_disc_radius")

        # Constraint for dynamic obstacles
        for obs_id in range(self.max_obstacles):
            obst_x = params.get(f"ellipsoid_obst_{obs_id}_x")
            obst_y = params.get(f"ellipsoid_obst_{obs_id}_y")
            obstacle_cog = np.array([obst_x, obst_y])

            obst_psi = params.get(f"ellipsoid_obst_{obs_id}_psi")
            obst_major = params.get(f"ellipsoid_obst_{obs_id}_major")
            obst_minor = params.get(f"ellipsoid_obst_{obs_id}_minor")
            obst_r = params.get(f"ellipsoid_obst_{obs_id}_r")

            # multiplier for the risk when obst_major, obst_major only denote the covariance
            # (i.e., the smaller the risk, the larger the ellipsoid).
            # This value should already be a multiplier (using exponential cdf).
            chi = params.get(f"ellipsoid_obst_{obs_id}_chi")

            # Compute ellipse matrix
            obst_major *= cd.sqrt(chi)
            obst_minor *= cd.sqrt(chi)
            ab = cd.SX(2, 2)
            ab[0, 0] = 1.0 / ((obst_major + r_disc + obst_r) * (obst_major + r_disc + obst_r))
            ab[0, 1] = 0.0
            ab[1, 0] = 0.0
            ab[1, 1] = 1.0 / ((obst_minor + r_disc + obst_r) * (obst_minor + r_disc + obst_r))

            obstacle_rotation = cd.SX(rotation_matrix(obst_psi))
            obstacle_ellipse_matrix = obstacle_rotation.T @ ab @ obstacle_rotation

            for disc_it in range(self.n_discs):
                # Get and compute the disc position
                disc_x = params.get(f"ego_disc_{disc_it}_offset")
                disc_relative_pos = np.array([disc_x, 0])
                disc_pos = pos + rotation_car @ disc_relative_pos

                # construct the constraint and append it
                disc_to_obstacle = cd.SX(disc_pos - obstacle_cog)
                c_disc_obstacle = disc_to_obstacle.T @ obstacle_ellipse_matrix @ disc_to_obstacle
                constraints.append(c_disc_obstacle)  # + slack)

        return constraints

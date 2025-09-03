#!/bin/bash
# Usage: ./build.sh <system_name> <generate_solver>

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$HOME/mpc_planner_syscon/acados/lib"
export ACADOS_SOURCE_DIR="$HOME/mpc_planner_syscon/acados"

. /opt/ros/noetic/setup.sh

# Source the workspace if it exists
if [ -f $HOME/mpc_planner_syscon/devel/setup.sh ]; then
  . $HOME/mpc_planner_syscon/devel/setup.sh
fi

# Generate a solver if enabled
if [ -z "$2" ]; then
  echo "Not rebuilding the solver."
else
  # Check if the argument equals "True" or "true"
  if [ "$2" = "True" ] || [ "$2" = "true" ]; then
    cd src/mpc_planner
    python3 -m poetry run python mpc_planner_$1/scripts/generate_$1_solver.py

    if  [ ${PIPESTATUS[0]} -eq 0 ]
    then
      echo "Solver built successfully."
    else
      echo "Failed to build the solver."
      exit 0
    fi
    cd ../..

  else
    echo "Not building the solver."
  fi
fi

BUILD_TYPE=RelWithDebInfo # Release, Debug, RelWithDebInfo, MinSizeRel
catkin config --cmake-args -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DPYTHON_VERSION=3

catkin build mpc_planner_$1

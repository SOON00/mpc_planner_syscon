install_poetry() {
    echo "Installing Poetry ..."
    python3 -m pip install poetry
    echo "Installed Poetry succesfully."
    python3 -m poetry lock
}

echo "Checking the poetry installation"
cd src/mpc_planner
if ! python3 -m poetry lock; then
    echo "Poetry is not installed."
    if [[ "$1" == "-y" ]]; then
        install_poetry
    else
         # Ask the user for confirmation
        read -p "Do you want to install Poetry now? (y/n): " response
        case "$response" in
            [yY][eE][sS]|[yY])
                install_poetry
                ;;
            [nN][oO]|[nN])
                echo "Installation aborted."
                exit 0
                ;;
            *)
                echo "Invalid input. Please enter y or n."
                exit 0
                ;;
        esac
    fi
fi
cd ../..

echo "Poetry is installed."
cd src/mpc_planner
python3 -m poetry install --no-interaction --no-root
python3 -m poetry add -e ../../acados/interfaces/acados_template
cd ../..
echo "Done."


# Install dependencies
sudo apt-get update -y
rosdep update --include-eol-distros
rosdep install -y -r --from-paths src --ignore-src --rosdistro $ROS_DISTRO

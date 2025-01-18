#!/usr/bin/env bash

set -e  # Exit on any error

# Color setup
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print with color
print_status() {
    echo -e "${GREEN}[+]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[!]${NC} $1"
}

# Check for Python 3.8+
check_python() {
    if ! command -v python3 >/dev/null 2>&1; then
        print_error "Python 3 is required but not installed."
        exit 1
    fi
    
    local python_version=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
    local major_version=$(echo "$python_version" | cut -d. -f1)
    local minor_version=$(echo "$python_version" | cut -d. -f2)
    
    if [ "$major_version" -lt 3 ] || ([ "$major_version" -eq 3 ] && [ "$minor_version" -lt 8 ]); then
        print_error "Python 3.8 or higher is required. Found version $python_version"
        exit 1
    fi
    print_status "Found Python $python_version"
}

# Check for pip
check_pip() {
    if ! command -v pip3 >/dev/null 2>&1; then
        print_error "pip3 is required but not installed."
        print_warning "Try: python3 -m ensurepip --upgrade"
        exit 1
    fi
    print_status "Found pip3"
}

# Check for system dependencies
check_dependencies() {
    local missing_deps=()
    
    # Check for required system packages
    for pkg in "python3-dev" "python3-pip" "build-essential"; do
        if ! dpkg -l | grep -q "^ii  $pkg "; then
            missing_deps+=("$pkg")
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_warning "Missing system dependencies: ${missing_deps[*]}"
        if [ "$EUID" -eq 0 ]; then
            print_status "Installing missing dependencies..."
            apt-get update && apt-get install -y "${missing_deps[@]}"
        else
            print_warning "Please install missing dependencies with:"
            print_warning "sudo apt-get install ${missing_deps[*]}"
            exit 1
        fi
    fi
}

# Install the package
install_package() {
    local install_dir="$1"
    
    print_status "Installing extended-dtmf package..."
    
    if [ -n "${VIRTUAL_ENV}" ]; then
        print_status "Detected running in virtual environment, installing directly..."
        pip install .
    else
        if [ "$install_dir" = "global" ]; then
            print_status "Installing globally..."
            pip3 install .
        else
            print_status "Installing in user space..."
            pip3 install --user .
        fi
    fi
    
    print_status "Installation complete!"
}

# Main installation logic
main() {
    print_status "Starting extended-dtmf installation..."
    
    # Determine if we're on a Debian-based system
    if command -v apt-get >/dev/null 2>&1; then
        if [ "$EUID" -eq 0 ]; then
            check_dependencies
        else
            print_warning "Running in user mode, skipping system dependency check"
        fi
    else
        print_warning "Non-Debian system detected, skipping system dependency check"
    fi
    
    check_python
    check_pip
    
    # Determine installation mode
    if [ "$EUID" -eq 0 ]; then
        print_status "Installing globally..."
        install_package "global"
    else
        print_warning "Installing in user mode..."
        install_package "user"
    fi
    
    # Verify installation
    if command -v dtmf >/dev/null 2>&1; then
        print_status "Successfully installed extended-dtmf!"
        print_status "Try running: dtmf --help"
    else
        print_error "Installation seems to have failed. Please check the error messages above."
        exit 1
    fi
}

# Run the installer
main "$@"
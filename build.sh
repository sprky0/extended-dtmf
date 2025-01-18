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

# Create and activate virtual environment for building
print_status "Creating build environment..."
python3 -m venv build_env
source build_env/bin/activate

# Install build requirements
print_status "Installing build dependencies..."
pip install -r requirements-build.txt

# Install our package in editable mode
print_status "Installing package..."
pip install -e .

# Build binary
print_status "Building binary..."
python build_binary.py

# Deactivate and cleanup
deactivate
rm -rf build_env

print_status "Build complete! Binary can be found in dist/dtmf"
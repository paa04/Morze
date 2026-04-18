#!/bin/bash
set -e

BUILD_DIR="cmake-build-tests"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build . --target MorzeTests -j$(nproc)

echo "Running tests..."
./MorzeTests
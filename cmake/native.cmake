cmake_minimum_required(VERSION 3.28)
project(boreline_native)

# This file lists native compiler definitions used before the full CMake
# toolchain file is generated. Build configuration is fully driven by
# the top-level CMakeLists.txt; this file documents the supported
# compilers and flags for CI.

# Minimum verified compilers:
# - GCC 14 (C23 support: complete)
# - Clang 17 (C23 support: complete)
# - Apple Clang 16 (macOS client/shared)

message(STATUS "boreline native build configuration")
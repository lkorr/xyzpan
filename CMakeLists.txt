cmake_minimum_required(VERSION 3.25)
project(XYZPan VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CPM dependency manager
include(cmake/CPM.cmake)

# Dependencies
CPMAddPackage("gh:juce-framework/JUCE#8.0.12")
CPMAddPackage("gh:catchorg/Catch2@3.7.1")
CPMAddPackage("gh:g-truc/glm#1.0.1")

# Subdirectories (order matters: engine must come before plugin and tests)
add_subdirectory(engine)
add_subdirectory(plugin)
add_subdirectory(ui)
add_subdirectory(tests)

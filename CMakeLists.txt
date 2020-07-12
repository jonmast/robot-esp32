# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# include_directories(${PROJECT_SOURCE_DIR})
# set(COMPONENT_SRCS "main/motor.c")

project(robot-esp32 C)

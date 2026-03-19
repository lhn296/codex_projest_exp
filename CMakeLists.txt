# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH is not set. Configure ESP-IDF and reopen the project from an ESP-IDF environment.")
endif()

set(PROJECT_VER "v1.9.0")
set(COMPONENTS main app bsp driver services system)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(codex_project_tep)

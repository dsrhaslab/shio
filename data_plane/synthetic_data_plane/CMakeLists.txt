# ---------------------------------------------------------------------------- #

cmake_minimum_required(VERSION 3.11)
include(FetchContent)

# ---------------------------------------------------------------------------- #

project(shepherd VERSION 1.0.0 DESCRIPTION "Shepherd: A Software-Defined Storage Control Plane")

# ---------------------------------------------------------------------------- #
# include(../cmake/common.cmake)
cmake_minimum_required(VERSION 3.5.1)

set (CMAKE_CXX_STANDARD 17)

if(MSVC)
    add_definitions(-D_WIN32_WINNT=0x600)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------- #


# ---------------------------------------------------------------------------- #
add_executable(data_plane_stage src/client/data_plane_stage.cpp
        #include/utils/context_propagation_definitions.hpp
        include/utils/interface_definitions.hpp
        )

target_include_directories(data_plane_stage PUBLIC include)

# ---------------------------------------------------------------------------- #

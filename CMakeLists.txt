cmake_minimum_required(VERSION 3.20)
project(RbfxScript)

# C++17 is required for this project.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup output directories.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Configure engine options. This is required for in-source engine builds.
set(URHO3D_SAMPLES OFF)

# Find the engine. The user can set CMAKE_PREFIX_PATH to override the default.
# The default is the sibling rbfx checkout, or a fixed path as a last resort.
if(NOT CMAKE_PREFIX_PATH)
    get_filename_component(_RBFX_SCRIPT_SIBLING_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/../rbfx/CMake" ABSOLUTE)
    if(EXISTS "${_RBFX_SCRIPT_SIBLING_CMAKE}")
        set(_RBFX_SCRIPT_DEFAULT_PREFIX "${_RBFX_SCRIPT_SIBLING_CMAKE}")
    else()
        set(_RBFX_SCRIPT_DEFAULT_PREFIX "/home/rk/src/rbfx/rbfx/CMake")
    endif()
    set(RBFX_SCRIPT_ENGINE_PREFIX "${_RBFX_SCRIPT_DEFAULT_PREFIX}" CACHE PATH
        "Default engine prefix path. It applies only when CMAKE_PREFIX_PATH is empty.")
    list(APPEND CMAKE_PREFIX_PATH "${RBFX_SCRIPT_ENGINE_PREFIX}")
endif()
find_package(Urho3D REQUIRED)

# Hot++ options. Set them before add_subdirectory. HOT_ENABLE_JIT stays at AUTO.
set(HOT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HOT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(HOT_INSTALL OFF CACHE BOOL "" FORCE)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hotpp/CMakeLists.txt")
    message(FATAL_ERROR
        "Missing hotpp/CMakeLists.txt. Run: git submodule update --init --recursive")
endif()
add_subdirectory(hotpp)

# Game plugin.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/Rbfx.Script/CMakeLists.txt")
    add_subdirectory(Plugins/Rbfx.Script)
endif()

# Editor plugin.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/Rbfx.Script.Editor/CMakeLists.txt")
    add_subdirectory(Plugins/Rbfx.Script.Editor)
endif()

# Custom editor linked with the plugins.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Editor/CMakeLists.txt")
    add_subdirectory(Source/Editor)
endif()

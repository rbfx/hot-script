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
# The Hot++ translator uses libclang, and libclang needs the Clang resource folder (stddef.h and
# the other compiler headers). Hot++ asks the compiler of the build for the folder, but only a
# Clang build can answer. For a GCC build, ask the clang++ of the system.
# The backend also gives the script compiler the flag -resource-dir, and GCC does not know it.
# So a GCC build compiles the scripts with clang++. It uses the same C++ library as GCC.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
    find_program(RBFX_SCRIPT_CLANGXX NAMES clang++)
    if(RBFX_SCRIPT_CLANGXX)
        if(NOT HOT_CLANG_RESOURCE_DIR)
            execute_process(COMMAND ${RBFX_SCRIPT_CLANGXX} -print-resource-dir
                            OUTPUT_VARIABLE _rbfx_script_resource_dir OUTPUT_STRIP_TRAILING_WHITESPACE)
            set(HOT_CLANG_RESOURCE_DIR "${_rbfx_script_resource_dir}" CACHE PATH "The Clang resource directory")
        endif()
        if(NOT HOT_SCRIPT_COMPILER OR HOT_SCRIPT_COMPILER STREQUAL CMAKE_CXX_COMPILER)
            set(HOT_SCRIPT_COMPILER "${RBFX_SCRIPT_CLANGXX}" CACHE STRING
                "The compiler of the script modules at run time" FORCE)
        endif()
    else()
        message(WARNING "clang++ is not found. The run-time compile of the scripts needs it.")
    endif()
endif()

# The plugins are shared libraries when the engine is shared. They link the Hot++ static
# libraries, so these libraries need position independent code.
if(BUILD_SHARED_LIBS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
add_subdirectory(hotpp)

# A script module reads the call depth of Hot++ (a thread-local variable) with the initial-exec
# model. The engine opens a plugin with dlopen. Then glibc gives the plugin static TLS only when
# the plugin itself uses the initial-exec model. This flag marks the plugin STATIC_TLS.
if(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(target hot_runtime_core hot_runtime)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE -ftls-model=initial-exec)
        endif()
    endforeach()
endif()

# Shared script core. The game plugin and the editor plugin link it.
add_subdirectory(Plugins/Rbfx.Script.Core)

# Game plugin.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/Rbfx.Script/CMakeLists.txt")
    add_subdirectory(Plugins/Rbfx.Script)
endif()

# End-to-end test of the game plugin.
option(RBFX_SCRIPT_BUILD_TESTS "Build the end-to-end test of the game plugin" OFF)
if(RBFX_SCRIPT_BUILD_TESTS)
    add_subdirectory(Tests/ScriptSmokeTest)
endif()

# Editor plugin.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/Rbfx.Script.Editor/CMakeLists.txt")
    add_subdirectory(Plugins/Rbfx.Script.Editor)
endif()

# Main plugin of the demo project and its player.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Application/CMakeLists.txt")
    add_subdirectory(Source/Application)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Launcher/CMakeLists.txt")
    add_subdirectory(Source/Launcher)
endif()

# Custom editor linked with the plugins.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Editor/CMakeLists.txt")
    add_subdirectory(Source/Editor)
endif()

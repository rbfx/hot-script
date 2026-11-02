# RbfxScript.Main: the main plugin of the demo project. It loads a scene and shows it.
# It is not a part of the scripting system. Plugin.Rbfx.Script stays a generic plugin.
file(GLOB RBFX_SCRIPT_MAIN_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(RbfxScript.Main ${RBFX_SCRIPT_MAIN_SOURCES})
target_link_libraries(RbfxScript.Main PRIVATE Urho3D)
setup_plugin_target(RbfxScript.Main RbfxScript.Main)
# The project loads this plugin after Plugin.Rbfx.Script. Build them together.
if(TARGET Plugin.Rbfx.Script)
    add_dependencies(RbfxScript.Main Plugin.Rbfx.Script)
endif()

# RbfxScriptPlayer: the player of the demo project without the editor.
if(NOT TARGET PlayerLibrary OR NOT TARGET Plugin.Rbfx.Script OR NOT TARGET RbfxScript.Main)
    return()
endif()

add_executable(RbfxScriptPlayer Launcher.cpp)
target_link_libraries(RbfxScriptPlayer PRIVATE Urho3D PlayerLibrary)
# The order is the load order. It is the same as in Project/Project.json.
target_link_plugins(RbfxScriptPlayer "Plugin.Rbfx.Script;RbfxScript.Main")
add_dependencies(RbfxScriptPlayer Plugin.Rbfx.Script RbfxScript.Main)

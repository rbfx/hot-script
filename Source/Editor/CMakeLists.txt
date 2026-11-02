# RbfxScriptEditor: the editor of rbfx with the "Scripts" tab of Plugin.Rbfx.Script.Editor.
if(NOT TARGET EditorLibrary OR NOT TARGET Plugin.Rbfx.Script.Editor)
    return()
endif()

if(NOT URHO3D_WIN32_CONSOLE)
    set(RBFX_SCRIPT_EDITOR_TYPE WIN32)
endif()
add_executable(RbfxScriptEditor ${RBFX_SCRIPT_EDITOR_TYPE} Editor.cpp)
target_link_libraries(RbfxScriptEditor PRIVATE Plugin.Rbfx.Script.Editor)
# The game plugin and the demo main plugin are shared libraries that the project loads (Project/Project.json).
# They are not linked. The build makes them before the editor, so the editor can find them.
target_link_plugins(RbfxScriptEditor "")
foreach(plugin Plugin.Rbfx.Script RbfxScript.Main)
    if(TARGET ${plugin})
        add_dependencies(RbfxScriptEditor ${plugin})
    endif()
endforeach()

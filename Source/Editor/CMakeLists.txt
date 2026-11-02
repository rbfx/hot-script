# RbfxScriptEditor: the editor of rbfx with the "Scripts" tab of Plugin.Rbfx.Script.Editor.
if(NOT TARGET EditorLibrary OR NOT TARGET Plugin.Rbfx.Script.Editor)
    return()
endif()

if(NOT URHO3D_WIN32_CONSOLE)
    set(RBFX_SCRIPT_EDITOR_TYPE WIN32)
endif()
add_executable(RbfxScriptEditor ${RBFX_SCRIPT_EDITOR_TYPE} Editor.cpp)
target_link_libraries(RbfxScriptEditor PRIVATE Plugin.Rbfx.Script.Editor)
# The game plugin is a shared library that the project loads (Project/Project.json).
# It is not linked. The build makes it before the editor, so the editor can find it.
target_link_plugins(RbfxScriptEditor "")
if(TARGET Plugin.Rbfx.Script)
    add_dependencies(RbfxScriptEditor Plugin.Rbfx.Script)
endif()

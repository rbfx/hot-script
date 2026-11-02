# Plugin.Rbfx.Script.Editor: the editor side of the scripts. It needs the editor library.
# The custom editor (Source/Editor) links it and registers RbfxScript_ScriptsTab.
if(NOT TARGET EditorLibrary)
    return()
endif()

add_library(Plugin.Rbfx.Script.Editor STATIC
    DiskScriptSource.h DiskScriptSource.cpp
    ScriptsTab.h ScriptsTab.cpp)
target_link_libraries(Plugin.Rbfx.Script.Editor PUBLIC EditorLibrary Rbfx.Script.Core)
target_include_directories(Plugin.Rbfx.Script.Editor PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)

# The end-to-end test of the game plugin. It needs a compiler at run time.
# The test uses the ScriptSystem of the plugin through its virtual functions. It does not link
# Rbfx.Script.Core: a second copy of the Hot++ runtime state in the program breaks the reload.
add_executable(Rbfx.Script.SmokeTest main.cpp)
target_include_directories(Rbfx.Script.SmokeTest PRIVATE
    $<TARGET_PROPERTY:Rbfx.Script.Core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(Rbfx.Script.SmokeTest PRIVATE
    $<TARGET_PROPERTY:Rbfx.Script.Core,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(Rbfx.Script.SmokeTest PRIVATE Urho3D)
add_dependencies(Rbfx.Script.SmokeTest Plugin.Rbfx.Script)

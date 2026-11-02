# The end-to-end test of the game plugin. It needs a compiler at run time.
add_executable(Rbfx.Script.SmokeTest main.cpp)
target_link_libraries(Rbfx.Script.SmokeTest PRIVATE Rbfx.Script.Core)
add_dependencies(Rbfx.Script.SmokeTest Plugin.Rbfx.Script)

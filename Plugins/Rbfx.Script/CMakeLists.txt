# Plugin.Rbfx.Script: the game plugin. It reads the scripts from the ResourceCache.
# The plugin is a shared library when the engine is shared, and a static library otherwise.

file(GLOB RBFX_SCRIPT_PLUGIN_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_plugin(Plugin.Rbfx.Script "${RBFX_SCRIPT_PLUGIN_SOURCES}")
target_link_libraries(Plugin.Rbfx.Script PRIVATE Rbfx.Script.Core)

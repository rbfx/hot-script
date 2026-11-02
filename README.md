# rbfx-script

Script support for rbfx. It uses Hot++ for hot reload.

## Layout

- `hotpp/`: Hot++ submodule.
- `Plugins/Rbfx.Script.Core/`: shared script core (static library `Rbfx.Script.Core`).
- `Plugins/Rbfx.Script/`: game plugin (`Plugin.Rbfx.Script`).
- `Plugins/Rbfx.Script.Editor/`: editor plugin.
- `Source/Editor/`: custom editor.
- `Tests/ScriptSmokeTest/`: end-to-end test of the game plugin.

## Build

    git submodule update --init --recursive
    cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

The default engine path is `../rbfx/CMake`.
Set `-DCMAKE_PREFIX_PATH=<path>` to use a different engine.

## Architecture

`Rbfx.Script.Core` has no plugin entry point. It has these parts:

- `IScriptSource`: the origin of the scripts. It lists the scripts, reads a file and gives a modification stamp.
- `ScriptSourceFileSystem`: the Hot++ file system (`hot::FileSystem`) over an `IScriptSource`.
  A script has the backend name `/rbfx-script-source/<name>`.
  So a `using` line or an `#include` between scripts also reads through the source.
- `ScriptSystem`: an engine subsystem. It owns the Hot++ backend and the Hot++ loader.
  Each script is one Hot++ unit.
  `LoadScript`, `ReloadScript` and `LoadAllScripts` compile a script and commit it at once.
  `CreateObject<T>(className)` makes an object of a script class with the native base `T`.
  Errors go to the log with `URHO3D_LOGERROR`.

The engine include folders and definitions come from the `Urho3D` target at configure time.
CMake writes them to a generated file. The script compile uses them.

The game plugin `Plugin.Rbfx.Script` registers the `ScriptSystem` subsystem in `Load`.
Its source is `ResourceScriptSource`: the `.hot` and `.cpp` files of the `Scripts` resource folder.
The plugin loads all these scripts in `Load`.
The editor plugin supplies its own source and uses the same core.

### Hot reload

The subsystem polls the stamp of each loaded script every 500 ms, in the `E_ENDFRAME` handler.
The game source uses the modification time of the resolved file on the disk.
A file in a package has the stamp zero and does not reload.
A changed script compiles again, and the new code goes live at the end of the frame.
A changed header does not start a reload. Call `ReloadScript` for it.

### Rules

- Call `LoadScript` and `ReloadScript` on the main thread, and not from script code.
- The factory of a script class takes no arguments.
  A script class with an engine base gets the context from `Context::GetInstance()`.
- Release all script objects before the plugin unloads.
- The plugin opens its own library again with `RTLD_GLOBAL`.
  This lets the script modules find the Hot++ runtime in the plugin.
- A GCC build compiles the scripts with the `clang++` of the system.
  The run-time compile needs `clang++` and its resource folder.

### Smoke test

    cmake -S . -B cmake-build -DRBFX_SCRIPT_BUILD_TESTS=ON
    cmake --build cmake-build --target Rbfx.Script.SmokeTest
    cp -r Tests/ScriptSmokeTest/Data /tmp/script-data
    cmake-build/bin/Rbfx.Script.SmokeTest cmake-build/bin/libPlugin.Rbfx.Script.so /tmp/script-data

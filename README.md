# rbfx-script

Script support for rbfx. It uses Hot++ for hot reload.

## Layout

- `hotpp/`: Hot++ submodule.
- `Plugins/Rbfx.Script.Core/`: script core `Rbfx.Script.Core`. It is a shared library when the engine is shared.
- `Plugins/Rbfx.Script/`: game plugin (`Plugin.Rbfx.Script`).
- `Plugins/Rbfx.Script.Editor/`: editor plugin (`Plugin.Rbfx.Script.Editor`, a static library for the custom editor).
- `Source/Editor/`: custom editor `RbfxScriptEditor`.
- `Source/Application/`: demo main plugin `RbfxScript.Main`. It is not a part of the scripting system.
- `Source/Launcher/`: demo player `RbfxScriptPlayer`.
- `Tests/ScriptSmokeTest/`: end-to-end test of the game plugin.
- `Project/`: editor demo project. `Project/Data/Scripts/` has the sample scripts.

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
The editor plugin has the source `DiskScriptSource`: one folder on the disk, without the ResourceCache.
It adds the "Scripts" tab to the editor.
The tab shows the script folder, the scripts, their classes, the generation and the compile errors.
The Reload button compiles all scripts of the folder again.
The default folder is `<project data folder>/Scripts`.
Type a folder relative to the project folder and click "Apply Folder" to change it.
The project `ui.ini` keeps the folder (entry `ScriptDir`).

### The prelude

`Plugins/Rbfx.Script.Core/Prelude/Rbfx.prelude.hot` configures the scripts.
CMake embeds the file in the core library, so the game needs no file for it.
The `ScriptSystem` gives it to the Hot++ backend (`hot::DefaultBackendOptions::preludes`).

- `[std("eastl")]`: `std.vector<int>` in a script is `eastl::vector<int>`.
- A class that derives from `Urho3D::RefCounted` is a `Urho3D::SharedPtr` handle.
  `Urho3D.Node n = new Urho3D.Node(ctx);` holds a `Urho3D::SharedPtr<Urho3D::Node>`.
- `[weak] Urho3D.Node w` is a `Urho3D::WeakPtr<Urho3D::Node>`. A read gives null when the object is gone.
- A script class with a `RefCounted` native base, such as `Urho3D.LogicComponent`, uses the same handles.
- A raw pointer `Urho3D.Node*` stays a raw pointer.

A script class without a native base uses `eastl::shared_ptr`.
In such a class, `this` as a handle is not supported.
Give the class a native base that derives from `RefCounted` instead.

`Project/Data/Scripts/Spawner.hot` is the sample.
The smoke test checks that its weak handle gives null after the node is removed.

### Reflected script types

The prelude declares the Hot++ script attribute `ReflectedObject`.
Put it on a `.hot` script class to register the class with the engine:

```hot
[ReflectedObject]
class Rotator : Urho3D.LogicComponent { ... }
```

A `.cpp` script cannot use the `.hot` attribute syntax.
Include `Rbfx.Script.Core/ScriptReflection.h` and write `RBFX_SCRIPT_REFLECTED` after `class`:

```cpp
#include <Rbfx.Script.Core/ScriptReflection.h>

class RBFX_SCRIPT_REFLECTED Rotator2 : public Urho3D::LogicComponent { ... };
```

The macro is the Hot++ macro `HOT_ATTRIBUTE("ReflectedObject")`, a clang annotation.
The two forms give the same result.

- After each script load, the `ScriptSystem` registers each new class with this attribute.
- The engine factory makes the class by its name: `Context::CreateObject` and `Node::CreateComponent`.
- The engine attributes of the native base are copied to the script type.
- A component type gets the category `Component/User`. The editor lists it in the "User" group of its component menus.
- The primary native base must be an engine type, for example `LogicComponent`.
- The script system loads the scripts in name order. It registers the types before the plugin `Load` returns.
  Thus the types exist before a scene loads, also after a plugin reload of the editor.
- The destructor of the `ScriptSystem` unregisters its types (`UnregisterReflectedTypes`).
  So a plugin unload removes the types, and a new plugin instance registers them again.
- Here, "attribute" is a Hot++ tag. An "engine attribute" is a reflected rbfx property.

Script type identity:

- The prelude puts `URHO3D_OBJECT(ScriptClass, Base)` in the shell of each script class below `Urho3D::Object`.
  This applies to all such classes, also to classes without `ReflectedObject`.
- So `GetType`, `GetTypeName`, `GetTypeInfo` and `IsInstanceOf` give the script type.
  A script cannot override these four functions.
- The engine registers the static `TypeInfo` of the shell. An object and its registered type have the same `TypeInfo`.
  A hot reload keeps it. A new plugin instance registers a new one.
  Exception: after a reload that changes the constructors of a class, new objects have a second `TypeInfo`.
  It has the same name and type hash, so a scene save and the engine factory still work.
- A scene save writes the script type, and a scene load makes the script class again.
- `Node::GetComponent<LogicComponent>()` does not find a script component, because it compares the exact type.
  Use `GetDerivedComponent<LogicComponent>()` or the script type name.
- A scene that was saved before this change has the base type (for example `LogicComponent`) for its script components.
  Such a component loads as an `UnknownComponent`. Remove it and add the script component again.

Limits (TODO comments in `ScriptSystem::RegisterReflectedTypes`):

- A removed class does not unregister the type.
- Script fields are not engine attributes yet.

`Rotator.hot`, `Spawner.hot` and `Rotator2.cpp` in `Project/Data/Scripts` are the samples.
The smoke test checks that the engine knows them and makes them by name.

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
- The process must have one copy of the Hot++ runtime state.
  With a shared engine, `Rbfx.Script.Core` is a shared library, and all plugins link it.
  Each plugin calls `ScriptSystem::GetOrCreateShared`.
  It registers a `ScriptSystem` only when the context has none.
  The plugin that makes the subsystem removes it. The other plugin uses it.
  With a static engine, link the core into one module only.
  The operations of `ScriptSystem` are virtual, so a module with only the header calls the code of the core.
- Linux: the Hot++ runtime compiles with `-ftls-model=initial-exec`.
  This marks the plugin `STATIC_TLS`. So glibc gives the plugin static TLS when the engine opens it.
  The script modules read the call depth of Hot++ from static TLS.

### Editor demo

    cmake --build cmake-build --target RbfxScriptEditor
    cmake-build/bin/RbfxScriptEditor "$PWD/Project"

The editor build needs `URHO3D_EDITOR` (the `EditorLibrary` target).
The project folder is a positional argument of the editor.
The "Scripts" tab loads the scripts of `Project/Data/Scripts/`.
`Rotator.hot` and `Rotator2.cpp` are `LogicComponent` classes that turn their node.
`Project/Project.json` also loads the game plugin `Plugin.Rbfx.Script`, for the play mode.
The game plugin then uses the `ScriptSystem` of the tab. So the editor has one script system.

### Play mode

The play mode needs a main plugin. The main plugin loads the scene and makes the viewport.
`Plugin.Rbfx.Script` is generic, so it is not a main plugin.
The demo main plugin is `RbfxScript.Main` (`Source/Application/`).
It loads the scene of the `SceneName` engine parameter and uses the first camera.
If the scene has no camera, it creates a camera.
It moves the camera to the editor camera position and rotation.

`Project/Project.json` loads `Plugin.Rbfx.Script` first and `RbfxScript.Main` second.
Thus the script types exist when the scene loads.
The scene also stops before the scripts stop.

The player runs the same plugins without the editor:

    cmake --build cmake-build --target RbfxScriptPlayer
    cd cmake-build/bin && ./RbfxScriptPlayer --scene Scenes/Sample.scene

The player needs `ResourceRoot.ini` in its directory or in a parent directory.

### Smoke test

    cmake -S . -B cmake-build -DRBFX_SCRIPT_BUILD_TESTS=ON
    cmake --build cmake-build --target Rbfx.Script.SmokeTest
    cp -r Project/Data /tmp/script-data
    cmake-build/bin/Rbfx.Script.SmokeTest cmake-build/bin/libPlugin.Rbfx.Script.so /tmp/script-data

Give an absolute data folder. The test changes `Scripts/Rotator2.cpp` in the copy and restores it.
The test does not link `Rbfx.Script.Core`. It uses the `ScriptSystem` of the plugin.

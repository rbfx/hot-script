# rbfx-script

Script support for rbfx. It uses Hot++ for hot reload.

## Layout

- `hotpp/`: Hot++ submodule.
- `Plugins/Rbfx.Script/`: game plugin.
- `Plugins/Rbfx.Script.Editor/`: editor plugin.
- `Source/Editor/`: custom editor.

## Build

    git submodule update --init --recursive
    cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

The default engine path is `../rbfx/CMake`.
Set `-DCMAKE_PREFIX_PATH=<path>` to use a different engine.

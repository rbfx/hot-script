# rbfx-script

Script support for rbfx. It uses Hot++ for hot reload.
You write scripts as `.hot` or `.cpp` files. The game compiles them at run time and reloads them when they change.
A script class can be an engine type, for example a `LogicComponent`.

## Build

    git submodule update --init --recursive
    cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

The default engine path is `../rbfx/CMake`.
Set `-DCMAKE_PREFIX_PATH=<path>` to use a different engine.

## Documentation

See [docs/Architecture.md](docs/Architecture.md) for the layout, the IDE setup, the design and the tests.

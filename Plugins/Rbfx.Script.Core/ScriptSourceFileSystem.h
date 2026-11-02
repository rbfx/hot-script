// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include "Rbfx.Script.Core/ScriptSource.h"

#include <hot/vfs.h>

#include <memory>
#include <string>

namespace Urho3D
{

/// The Hot++ file system over an IScriptSource.
/// The script system gives each script the name "<VirtualRoot>/<name>" for the backend.
/// So a "using" line of a script searches this file system first, in the folder of the script.
/// A path outside the virtual root is not in this file system: the backend then reads the disk.
class ScriptSourceFileSystem final : public hot::FileSystem
{
public:
    /// The prefix of the script paths. No real folder has this name.
    static constexpr const char* VirtualRoot = "/rbfx-script-source";

    explicit ScriptSourceFileSystem(std::shared_ptr<IScriptSource> source);

    bool read(const std::string& path, std::string& out) override;
    hot::VfsStat stat(const std::string& path) override;

    /// Return the backend path of a source name.
    static std::string ToBackendPath(const ea::string& name);
    /// Return the source name of a backend path, or an empty string for a path outside the root.
    /// It removes the "." and ".." parts.
    static ea::string ToSourceName(const std::string& path);

private:
    std::shared_ptr<IScriptSource> source_;
};

} // namespace Urho3D

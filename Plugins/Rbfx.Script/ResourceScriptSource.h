// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include "_Plugin.h"

#include <Rbfx.Script.Core/ScriptSource.h>

#include <Urho3D/Container/Ptr.h>

namespace Urho3D
{

class Context;
class ResourceCache;

/// The script source of the game: the files in one resource folder of the ResourceCache.
/// The names are resource names, for example "Scripts/Rotator.hot".
/// The stamp is the modification time of the resolved file on the disk. A file in a package
/// has no such file, so it has the stamp zero and does not reload.
class PLUGIN_RBFX_SCRIPT_API ResourceScriptSource : public IScriptSource
{
public:
    /// Construct with the resource folder of the scripts, for example "Scripts".
    ResourceScriptSource(Context* context, const ea::string& resourceDir);

    ea::vector<ea::string> ListScripts() override;
    bool ReadText(const ea::string& name, ea::string& text) override;
    bool Exists(const ea::string& name) override;
    unsigned long long GetModificationStamp(const ea::string& name) override;
    ea::string GetDescription() const override;

private:
    WeakPtr<Context> context_;
    ea::string resourceDir_;
};

} // namespace Urho3D

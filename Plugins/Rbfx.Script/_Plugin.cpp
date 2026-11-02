// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "_Plugin.h"

#include "ResourceScriptSource.h"

#include <Rbfx.Script.Core/ScriptSystem.h>

#include <Urho3D/Core/Context.h>
#include <Urho3D/Plugins/PluginApplication.h>

#include <memory>

using namespace Urho3D;

namespace
{

/// The resource folder of the game scripts.
const char* const ScriptResourceDir = "Scripts";

/// The game plugin. It registers the ScriptSystem subsystem with the ResourceCache source,
/// and it loads all scripts of the "Scripts" resource folder.
class RbfxScriptPlugin : public Plugin
{
public:
    using Plugin::Plugin;

protected:
    void Load() override
    {
        auto source = std::make_shared<ResourceScriptSource>(context_, ScriptResourceDir);
        auto system = MakeShared<ScriptSystem>(context_, std::move(source));
        context_->RegisterSubsystem(system);
        system->LoadAllScripts();
    }

    void Unload() override
    {
        // The users of the subsystem must release the script objects before this point.
        context_->RemoveSubsystem<ScriptSystem>();
    }
};

} // namespace

URHO3D_DEFINE_PLUGIN_MAIN(RbfxScriptPlugin);

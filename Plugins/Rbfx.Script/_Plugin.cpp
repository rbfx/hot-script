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
/// and it loads all scripts of the "Scripts" resource folder. When the context has a
/// ScriptSystem already (for example the one of the editor plugin), the plugin uses it.
class RbfxScriptPlugin : public Plugin
{
public:
    using Plugin::Plugin;

protected:
    void Load() override
    {
        ScriptSystem* system = ScriptSystem::GetOrCreateShared(
            context_, [this] { return std::make_shared<ResourceScriptSource>(context_, ScriptResourceDir); },
            ownsSystem_);
        if (ownsSystem_)
            system->LoadAllScripts();
    }

    void Unload() override
    {
        // The users of the subsystem must release the script objects before this point.
        // A subsystem of another plugin stays.
        if (ownsSystem_)
            context_->RemoveSubsystem<ScriptSystem>();
        ownsSystem_ = false;
    }

private:
    /// True when this plugin made the subsystem.
    bool ownsSystem_{};
};

} // namespace

URHO3D_DEFINE_PLUGIN_MAIN(RbfxScriptPlugin);

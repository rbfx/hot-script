// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

// The main plugin of the demo project. It is not a part of the scripting system.
// Plugin.Rbfx.Script registers the script types. This plugin only shows a scene.
// Project/Project.json loads this plugin after Plugin.Rbfx.Script.
// Thus the scene loads when the script types exist, and the scene stops before the scripts stop.

#include "DemoGameScreen.h"

#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Engine/StateManager.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Plugins/Plugin.h>

namespace RbfxScriptDemo
{

class DemoApplication : public ExecutablePlugin
{
    URHO3D_OBJECT(DemoApplication, ExecutablePlugin);

public:
    explicit DemoApplication(Context* context)
        : ExecutablePlugin(context)
    {
    }

protected:
    /// Implement ExecutablePlugin.
    /// @{
    void Load() override {}

    void Start(bool isMain) override
    {
        if (!isMain)
            return;

        // The editor sets these parameters when it starts the play mode.
        // The launcher sets the scene name from the command line.
        auto engine = GetSubsystem<Engine>();
        StringVariantMap bundle;
        bundle[Param_SceneName] = engine->GetParameter(Param_SceneName);
        bundle[Param_ScenePosition] = engine->GetParameter(Param_ScenePosition);
        bundle[Param_SceneRotation] = engine->GetParameter(Param_SceneRotation);

        URHO3D_LOGINFO("DemoApplication: start");
        gameScreen_ = MakeShared<DemoGameScreen>(context_);
        GetSubsystem<StateManager>()->EnqueueState(gameScreen_, bundle);
    }

    void Stop() override
    {
        // The editor does not reset the state manager when the play mode stops.
        // Reset it here, so that the scene and the viewport do not stay active.
        if (gameScreen_)
        {
            GetSubsystem<StateManager>()->Reset();
            gameScreen_ = nullptr;
        }
    }

    void Unload() override { gameScreen_ = nullptr; }
    /// @}

private:
    SharedPtr<DemoGameScreen> gameScreen_;
};

} // namespace RbfxScriptDemo

URHO3D_EXPORT_PLUGIN(RbfxScriptDemo::DemoApplication);

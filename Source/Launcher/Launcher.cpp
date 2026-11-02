// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

// The player of the demo project. It loads Plugin.Rbfx.Script and RbfxScript.Main.
// Run it from the build directory. The engine finds ResourceRoot.ini in a parent directory.
//   RbfxScriptPlayer --scene Scenes/Sample.scene

#include <Urho3D/Core/CommandLine.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>

#include <Player/PlayerApplication.h>

using namespace Urho3D;

class RbfxScriptPlayerApplication : public PlayerApplication
{
public:
    using PlayerApplication::PlayerApplication;

    /// Implement PlayerApplication.
    /// @{
    void Setup() override
    {
        PlayerApplication::Setup();

#if DESKTOP
        GetCommandLineParser().add_option("--scene", sceneName_, "Scene to play.")->type_name("resource");
#endif
        engineParameters_[EP_PLUGINS] = ea::string::joined(LinkedPlugins::GetLinkedPlugins(), ";");
        LinkedPlugins::RegisterStaticPlugins();
    }

    void Start() override
    {
        // RbfxScript.Main reads the scene name from the same parameter as in the play mode of the editor.
        if (!sceneName_.empty())
            GetSubsystem<Engine>()->SetParameter(Param_SceneName, sceneName_);

        PlayerApplication::Start();
    }
    /// @}

private:
    ea::string sceneName_;
};

URHO3D_DEFINE_APPLICATION_MAIN(RbfxScriptPlayerApplication);

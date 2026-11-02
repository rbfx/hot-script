// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include <Urho3D/Engine/StateManager.h>
#include <Urho3D/Graphics/Viewport.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Scene.h>

namespace RbfxScriptDemo
{

using namespace Urho3D;

/// The game screen of the demo. It shows one scene through one camera.
/// The bundle of Activate() holds the scene name and the camera position and rotation.
/// The keys are Param_SceneName, Param_ScenePosition and Param_SceneRotation.
class DemoGameScreen : public ApplicationState
{
    URHO3D_OBJECT(DemoGameScreen, ApplicationState);

public:
    explicit DemoGameScreen(Context* context);
    ~DemoGameScreen() override;

    /// Implement ApplicationState.
    /// @{
    void Activate(StringVariantMap& bundle) override;
    void Deactivate() override;
    /// @}

    Scene* GetScene() const { return scene_; }

private:
    /// Load the scene and find or create the camera. Do nothing if the scene exists.
    void InitScene(const ea::string& sceneName);

    SharedPtr<Scene> scene_;
    WeakPtr<Node> cameraNode_;
};

} // namespace RbfxScriptDemo

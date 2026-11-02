// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "DemoGameScreen.h"

#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/IO/Log.h>

namespace RbfxScriptDemo
{

DemoGameScreen::DemoGameScreen(Context* context)
    : ApplicationState(context)
{
    SetMouseMode(MM_FREE);
    SetMouseVisible(true);
}

DemoGameScreen::~DemoGameScreen()
{
    scene_ = nullptr;
}

void DemoGameScreen::InitScene(const ea::string& sceneName)
{
    if (scene_)
        return;

    scene_ = MakeShared<Scene>(context_);
    if (sceneName.empty())
        URHO3D_LOGWARNING("DemoGameScreen: no scene name, the scene is empty");
    else if (scene_->LoadFile(sceneName))
        URHO3D_LOGINFO("DemoGameScreen: scene '{}' is loaded", sceneName);
    else
        URHO3D_LOGERROR("DemoGameScreen: cannot load scene '{}'", sceneName);

    // Use the first camera of the scene. Create a camera if the scene has no camera.
    auto camera = scene_->FindComponent<Camera>(ComponentSearchFlag::SelfOrChildrenRecursive);
    if (!camera)
    {
        Node* node = scene_->CreateChild("Demo Camera");
        node->SetPosition({0.0f, 5.0f, -10.0f});
        node->LookAt(Vector3::ZERO);
        camera = node->CreateComponent<Camera>();
        URHO3D_LOGINFO("DemoGameScreen: the scene has no camera, a camera is created");
    }
    cameraNode_ = camera->GetNode();

    SetViewport(0, MakeShared<Viewport>(context_, scene_, camera));
}

void DemoGameScreen::Activate(StringVariantMap& bundle)
{
    BaseClassName::Activate(bundle);

    const Variant& sceneName = bundle[Param_SceneName];
    InitScene(sceneName.IsEmpty() ? EMPTY_STRING : sceneName.GetString());

    // Move the camera to the camera position of the editor, if the editor gives one.
    if (const Variant& position = bundle[Param_ScenePosition]; !position.IsEmpty())
        cameraNode_->SetWorldPosition(position.GetVector3());
    if (const Variant& rotation = bundle[Param_SceneRotation]; !rotation.IsEmpty())
        cameraNode_->SetWorldRotation(rotation.GetQuaternion());

    // The scene updates itself on each E_UPDATE. Thus script components run while the screen is active.
    scene_->SetUpdateEnabled(true);
}

void DemoGameScreen::Deactivate()
{
    if (scene_)
        scene_->SetUpdateEnabled(false);

    BaseClassName::Deactivate();
}

} // namespace RbfxScriptDemo

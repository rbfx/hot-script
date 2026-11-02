// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

// The editor of rbfx with the "Scripts" tab. Open the demo project:
//   RbfxScriptEditor <path of rbfx-script>/Project

#include <Urho3D/Engine/EngineDefs.h>

#include <Editor/EditorApplication.h>

#include <Rbfx.Script.Editor/ScriptsTab.h>

using namespace Urho3D;

class RbfxScriptEditorApplication : public EditorApplication
{
public:
    using EditorApplication::EditorApplication;

    /// Implement EditorApplication.
    /// @{
    void Setup() override
    {
        editorPluginManager_->AddPlugin("RbfxScript.ScriptsTab", &RbfxScript_ScriptsTab);

        EditorApplication::Setup();
        LinkedPlugins::RegisterStaticPlugins();
        engineParameters_[EP_RESOURCE_ROOT_FILE] = "ResourceRoot.ini";
    }
    /// @}
};

URHO3D_DEFINE_APPLICATION_MAIN(RbfxScriptEditorApplication);

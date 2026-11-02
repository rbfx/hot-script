// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include <Editor/Project/EditorTab.h>
#include <Editor/Project/Project.h>

#include <memory>

namespace Urho3D
{

class DiskScriptSource;
class ScriptSystem;

/// The editor plugin function. Register it with EditorPluginManager::AddPlugin.
/// It adds the "Scripts" tab to each project.
void RbfxScript_ScriptsTab(Context* context, Project* project);

/// The "Scripts" tab. It shows the script folder, the loaded scripts and their classes, the last
/// generation and the compile errors. The Reload button compiles all scripts of the folder again.
///
/// The tab registers a ScriptSystem with a DiskScriptSource when the context has none. The
/// default folder is "<project data folder>/Scripts". The tab keeps a folder relative to the
/// project folder in the project ui.ini (entry "ScriptDir").
class ScriptsTab : public EditorTab
{
    URHO3D_OBJECT(ScriptsTab, EditorTab)

public:
    explicit ScriptsTab(Context* context);
    ~ScriptsTab() override;

    /// Implement EditorTab.
    /// @{
    void PreRenderUpdate() override;
    void RenderContent() override;
    void WriteIniSettings(ImGuiTextBuffer& output) override;
    void ReadIniSettings(const char* line) override;
    /// @}

    /// Make the script system and load the scripts. Project::OnInitialized calls it, after the
    /// project loaded the ui.ini and the plugins.
    void OnProjectInitialized();

private:
    /// Return the absolute script folder.
    ea::string GetAbsoluteDirectory() const;
    /// Return the ScriptSystem of the context. Make one when the context has none.
    ScriptSystem* EnsureScriptSystem();
    /// Use a new folder and load its scripts.
    void ApplyDirectory(const ea::string& relativeDirectory);

    /// The script folder relative to the project folder. Empty: "<data folder>/Scripts".
    ea::string relativeDirectory_;
    /// The text of the folder field.
    char directoryInput_[512]{};
    std::shared_ptr<DiskScriptSource> source_;
    /// True when this tab made the subsystem. Then the tab removes it.
    bool ownsSystem_{};
    bool projectLoaded_{};
};

} // namespace Urho3D

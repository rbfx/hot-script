// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "ScriptsTab.h"

#include "DiskScriptSource.h"

#include <Rbfx.Script.Core/ScriptSystem.h>

#include <Editor/Core/IniHelpers.h>

#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/SystemUI/SystemUI.h>

#include <cstring>

namespace Urho3D
{

namespace
{

const char* const DefaultScriptDir = "Scripts";

void CopyToBuffer(char* buffer, size_t size, const ea::string& text)
{
    std::strncpy(buffer, text.c_str(), size - 1);
    buffer[size - 1] = '\0';
}

} // namespace

void RbfxScript_ScriptsTab(Context* context, Project* project)
{
    auto tab = MakeShared<ScriptsTab>(context);
    project->AddTab(tab);
    project->OnInitialized.Subscribe(tab.Get(), &ScriptsTab::OnProjectInitialized);
}

ScriptsTab::ScriptsTab(Context* context)
    : EditorTab(context, "Scripts", "5d0b8f3e-6c2a-4b8e-9a51-3f7c2e8d4a10", EditorTabFlag::OpenByDefault,
          EditorTabPlacement::DockBottom)
{
}

ScriptsTab::~ScriptsTab()
{
    // The script objects of the project are released before the project closes.
    if (ownsSystem_)
        context_->RemoveSubsystem<ScriptSystem>();
}

ea::string ScriptsTab::GetAbsoluteDirectory() const
{
    Project* project = GetProject();
    if (!project)
        return {};
    if (relativeDirectory_.empty())
        return AddTrailingSlash(project->GetDataPath()) + DefaultScriptDir;
    if (IsAbsolutePath(relativeDirectory_))
        return relativeDirectory_;
    return AddTrailingSlash(project->GetProjectPath()) + relativeDirectory_;
}

ScriptSystem* ScriptsTab::EnsureScriptSystem()
{
    bool created = false;
    ScriptSystem* system = ScriptSystem::GetOrCreateShared(
        context_,
        [this]
        {
            if (!source_)
                source_ = std::make_shared<DiskScriptSource>(GetAbsoluteDirectory());
            return source_;
        },
        created);
    if (created)
    {
        ownsSystem_ = true;
        system->LoadAllScripts();
    }
    return system;
}

void ScriptsTab::ApplyDirectory(const ea::string& relativeDirectory)
{
    relativeDirectory_ = relativeDirectory;
    CopyToBuffer(directoryInput_, sizeof(directoryInput_), relativeDirectory_);
    if (!projectLoaded_)
        return;
    if (source_)
        source_->SetDirectory(GetAbsoluteDirectory());
    if (ScriptSystem* system = EnsureScriptSystem())
        system->LoadAllScripts();
}

void ScriptsTab::OnProjectInitialized()
{
    projectLoaded_ = true;
    EnsureScriptSystem();
}

void ScriptsTab::PreRenderUpdate()
{
    // The subsystem of another plugin can go away, for example when the game plugin unloads.
    if (projectLoaded_ && !context_->GetSubsystem<ScriptSystem>())
    {
        ownsSystem_ = false;
        EnsureScriptSystem();
    }
}

void ScriptsTab::RenderContent()
{
    auto* system = static_cast<ScriptSystem*>(context_->GetSubsystem(ScriptSystem::GetTypeStatic()));
    if (!system)
    {
        ui::TextUnformatted("No script system.");
        return;
    }

    ui::SetNextItemWidth(260.0f);
    ui::InputTextWithHint("##ScriptDir", DefaultScriptDir, directoryInput_, sizeof(directoryInput_));
    ui::SameLine();
    if (ui::Button("Apply Folder"))
        ApplyDirectory(directoryInput_);
    ui::SameLine();
    if (ui::Button("Reload"))
        system->LoadAllScripts();
    ui::SameLine();
    bool autoReload = system->GetAutoReload();
    if (ui::Checkbox("Auto Reload", &autoReload))
        system->SetAutoReload(autoReload);

    const IScriptSource* activeSource = system->GetSource();
    ui::Text("Folder: %s", activeSource ? activeSource->GetDescription().c_str() : "none");
    if (!ownsSystem_)
        ui::TextDisabled("Another plugin made the script system. The folder field has no effect.");
    ui::Text("Generation: %u", system->GetActiveGeneration());
    ui::Separator();

    const ea::vector<ScriptStatus> scripts = system->GetScriptStatus();
    if (scripts.empty())
        ui::TextUnformatted("No scripts.");
    for (const ScriptStatus& script : scripts)
    {
        const bool failed = !script.error_.empty();
        if (failed)
            ui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ui::Text("%s  %s  generation %u%s", script.name_.c_str(), script.loaded_ ? "loaded" : "not loaded",
            script.generation_, script.compiling_ ? "  compiling..." : "");
        if (failed)
            ui::PopStyleColor();
        ui::Indent();
        if (!script.classes_.empty())
            ui::Text("Classes: %s", ea::string::joined(script.classes_, ", ").c_str());
        if (failed)
        {
            ui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ui::TextWrapped("%s", script.error_.c_str());
            ui::PopStyleColor();
        }
        ui::Unindent();
    }
}

void ScriptsTab::WriteIniSettings(ImGuiTextBuffer& output)
{
    BaseClassName::WriteIniSettings(output);
    WriteStringToIni(output, "ScriptDir", relativeDirectory_);
}

void ScriptsTab::ReadIniSettings(const char* line)
{
    BaseClassName::ReadIniSettings(line);
    if (const auto value = ReadStringFromIni(line, "ScriptDir"))
        ApplyDirectory(*value);
}

} // namespace Urho3D

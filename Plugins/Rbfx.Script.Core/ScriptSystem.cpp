// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "Rbfx.Script.Core/ScriptSystem.h"

#include "Rbfx.Script.Core/ScriptSourceFileSystem.h"

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/IO/Log.h>

#include <hot/backend.h>
#include <hot/runtime/loader.h>
#include <hot/runtime/threads.h>

#include <exception>

#if !defined(_WIN32)
    #include <dlfcn.h>
#endif

namespace Urho3D
{

namespace
{

// The engine include folders and definitions of the build (kEngineIncludeDirs, kEngineDefines).
#include "Rbfx.Script.Core/ScriptBuildConfig.inl"

ea::string ToEaString(const std::string& text)
{
    return ea::string(text.data(), text.size());
}

ea::string FormatResult(const hot::ReloadResult& result)
{
    ea::string text = Format("{} ({})", hot::to_string(result.status), hot::to_string(result.reason));
    if (!result.error.empty())
        text += ": " + ToEaString(result.error);
    return text;
}

// The script modules resolve the Hot++ runtime and the host symbols in the global scope.
// The engine opens a plugin library without RTLD_GLOBAL, so the symbols of the Hot++ runtime
// in the plugin are not global. This function opens the module of this code again with
// RTLD_GLOBAL. For a program that links the core statically, the module is the program. Then
// the call has no effect, and the program needs ENABLE_EXPORTS.
void* PromoteOwnSymbols()
{
#if !defined(_WIN32)
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&PromoteOwnSymbols), &info) == 0 || !info.dli_fname)
        return nullptr;
    return dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD | RTLD_GLOBAL);
#else
    return nullptr;
#endif
}

} // namespace

ScriptSystem::ScriptSystem(Context* context, std::shared_ptr<IScriptSource> source)
    : Object(context)
    , source_(std::move(source))
{
    selfHandle_ = PromoteOwnSymbols();
    SubscribeToEvent(E_ENDFRAME, URHO3D_HANDLER(ScriptSystem, HandleEndFrame));
}

ScriptSystem::~ScriptSystem()
{
    // The loader unloads the script code. It goes before the backend (see the header).
    loader_.reset();
    backend_.reset();
    fileSystem_.reset();
#if !defined(_WIN32)
    if (selfHandle_)
        dlclose(selfHandle_);
#endif
}

void ScriptSystem::AddIncludeDir(const ea::string& path)
{
    extraIncludeDirs_.push_back(path);
}

void ScriptSystem::AddCompilerFlag(const ea::string& flag)
{
    extraFlags_.push_back(flag);
}

void ScriptSystem::ReportError(const ea::string& message)
{
    lastError_ = message;
    URHO3D_LOGERROR("ScriptSystem: {}", message);
}

bool ScriptSystem::EnsureBackend()
{
    if (backend_ && loader_)
        return true;
    if (backendFailed_)
        return false;
    if (!source_)
    {
        backendFailed_ = true;
        ReportError("no script source");
        return false;
    }

    hot::DefaultBackendOptions options;
    for (const char* const* dir = kEngineIncludeDirs; *dir; ++dir)
        options.include_dirs.emplace_back(*dir);
    for (const ea::string& dir : extraIncludeDirs_)
        options.include_dirs.emplace_back(dir.c_str());
    for (const char* const* define = kEngineDefines; *define; ++define)
        options.extra_flags.emplace_back(std::string("-D") + *define);
    for (const ea::string& flag : extraFlags_)
        options.extra_flags.emplace_back(flag.c_str());
    fileSystem_ = std::make_shared<ScriptSourceFileSystem>(source_);
    options.fs = fileSystem_;

    try
    {
        backend_ = hot::make_default_backend(options);
        loader_ = std::make_unique<hot::Loader>();
    }
    catch (const std::exception& error)
    {
        loader_.reset();
        backend_.reset();
        backendFailed_ = true;
        ReportError(Format("cannot create the Hot++ backend: {}", error.what()));
        return false;
    }

    URHO3D_LOGINFO("ScriptSystem: Hot++ backend '{}', scripts from {}", backend_->name(), source_->GetDescription());
    return true;
}

bool ScriptSystem::CompileAndApply(const ea::string& name)
{
    if (!EnsureBackend())
        return false;

    // Read the stamp before the text. A change between the two reads then starts one more reload.
    const unsigned long long stamp = source_->GetModificationStamp(name);
    ea::string text;
    if (!source_->ReadText(name, text))
    {
        ReportError(Format("cannot read the script '{}' from {}", name, source_->GetDescription()));
        return false;
    }

    hot::ScriptSource script;
    script.name = ScriptSourceFileSystem::ToBackendPath(name);
    script.text.assign(text.c_str(), text.size());

    // The generation must be newer than the active generation of the process.
    hot::LoadResult built = backend_->load(script, hot::active_generation + 1);
    if (built.module)
        built.module->unit = script.name;

    hot::ReloadResult result;
    // The first module of the loader goes through load(). Each later module is a reload.
    if (loader_->stats().generations.empty())
    {
        if (!built)
        {
            ReportError(Format("cannot compile '{}': {}", name, ToEaString(built.error)));
            return false;
        }
        result = loader_->load(std::move(*built.module));
    }
    else
    {
        result = loader_->request_reload(std::move(built));
        if (result.status == hot::ReloadStatus::Pending)
            result = loader_->commit_pending();
    }

    if (result.status != hot::ReloadStatus::Applied)
    {
        if (result.status == hot::ReloadStatus::FullReloadRequired)
            ReportError(Format("'{}' needs a full reload; release its objects and restart: {}", name, FormatResult(result)));
        else
            ReportError(Format("cannot load '{}': {}", name, FormatResult(result)));
        return false;
    }

    loaded_[name] = stamp;
    lastError_.clear();
    URHO3D_LOGINFO("ScriptSystem: loaded '{}' (generation {})", name, hot::active_generation);
    return true;
}

bool ScriptSystem::LoadScript(const ea::string& name)
{
    if (!IsScriptFileName(name))
    {
        ReportError(Format("'{}' is not a script: the name must end with .hot or .cpp", name));
        return false;
    }
    return CompileAndApply(name);
}

bool ScriptSystem::ReloadScript(const ea::string& name)
{
    if (!IsScriptLoaded(name))
    {
        ReportError(Format("cannot reload '{}': the script is not loaded", name));
        return false;
    }
    return CompileAndApply(name);
}

unsigned ScriptSystem::LoadAllScripts()
{
    if (!source_)
        return 0;
    unsigned count = 0;
    for (const ea::string& name : source_->ListScripts())
    {
        if (LoadScript(name))
            ++count;
    }
    return count;
}

void* ScriptSystem::CreateNative(const ea::string& className)
{
    if (!loader_)
    {
        ReportError(Format("cannot create '{}': no script is loaded", className));
        return nullptr;
    }
    const hot::ClassEntry* entry = loader_->find(className.c_str());
    if (!entry || !entry->create)
    {
        ReportError(Format("cannot create '{}': the class is not live", className));
        return nullptr;
    }
    return entry->create().native;
}

void ScriptSystem::PollScripts()
{
    ea::vector<ea::string> changed;
    for (const auto& [name, stamp] : loaded_)
    {
        const unsigned long long current = source_->GetModificationStamp(name);
        if (current != 0 && current != stamp)
            changed.push_back(name);
    }
    for (const ea::string& name : changed)
    {
        URHO3D_LOGINFO("ScriptSystem: '{}' changed, reloading", name);
        if (!CompileAndApply(name))
        {
            // Keep the old code, and do not compile the same text again at each poll.
            loaded_[name] = source_->GetModificationStamp(name);
        }
    }
}

void ScriptSystem::Update()
{
    if (!loader_)
        return;
    if (autoReload_ && pollTimer_.GetMSec(false) >= pollInterval_)
    {
        pollTimer_.Reset();
        PollScripts();
    }
    // A reload that an earlier call left pending goes live now.
    if (loader_->reload_pending())
    {
        const hot::ReloadResult result = loader_->commit_pending();
        if (result.status != hot::ReloadStatus::Applied)
            ReportError(Format("cannot commit a pending reload: {}", FormatResult(result)));
    }
    loader_->frame_boundary();
}

void ScriptSystem::HandleEndFrame(StringHash eventType, VariantMap& eventData)
{
    Update();
}

} // namespace Urho3D

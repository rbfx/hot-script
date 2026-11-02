// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "Rbfx.Script.Core/ScriptSystem.h"

#include "Rbfx.Script.Core/ScriptReflection.h"
#include "Rbfx.Script.Core/ScriptSourceFileSystem.h"

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/ObjectCategory.h>
#include <Urho3D/Core/ObjectReflection.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Scene/Component.h>

#include <EASTL/algorithm.h>
#include <EASTL/sort.h>

#include <hot/backend.h>
#include <hot/runtime/loader.h>
#include <hot/runtime/records.h>
#include <hot/runtime/threads.h>

#include <atomic>
#include <exception>
#include <thread>

#if !defined(_WIN32)
    #include <dlfcn.h>
#endif

namespace Urho3D
{

namespace
{

// The engine include folders and definitions of the build (kEngineIncludeDirs, kEngineDefines).
#include "Rbfx.Script.Core/ScriptBuildConfig.inl"
#include "ScriptPrelude.inl"

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

// The script attribute that registers a script class with the engine (see the prelude).
const char* const ReflectedObjectAttribute = RBFX_SCRIPT_REFLECTED_ATTRIBUTE;

// The engine type name of the primary native base. Hot++ gives the C++ names of the native bases
// with ", " between them, for example "Urho3D::LogicComponent, IFoo". The result is "LogicComponent".
ea::string EngineBaseName(const char* baseList)
{
    ea::string base = baseList ? baseList : "";
    const auto comma = base.find(',');
    if (comma != ea::string::npos)
        base.resize(comma);
    const auto colon = base.rfind(':');
    if (colon != ea::string::npos)
        base = base.substr(colon + 1);
    base.trim();
    return base;
}

// The engine factory of a reflected script class: the ScriptSystem makes the object by its name.
// The native pointer points to the primary native base. Object is at offset zero of it.
SharedPtr<Object> CreateReflectedObject(const TypeInfo* typeInfo, Context* context)
{
    auto* system = context->GetSubsystem<ScriptSystem>();
    return system ? system->CreateObject<Object>(typeInfo->GetTypeName()) : nullptr;
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

/// One compile of one script. The main thread reads the text and starts the job. The worker thread
/// runs only Backend::load. The main thread then applies the result.
struct ScriptSystem::CompileJob
{
    ea::string name_;
    /// The stamp of the source before the read of the text.
    unsigned long long stamp_{};
    hot::ScriptSource script_;
    uint32_t generation_{};
    hot::LoadResult built_;
    std::thread thread_;
    std::atomic<bool> done_{false};
};

ScriptSystem::ScriptSystem(Context* context, std::shared_ptr<IScriptSource> source)
    : Object(context)
    , source_(std::move(source))
{
    selfHandle_ = PromoteOwnSymbols();
    SubscribeToEvent(E_ENDFRAME, URHO3D_HANDLER(ScriptSystem, HandleEndFrame));
}

ScriptSystem::~ScriptSystem()
{
    // The worker thread runs code of this module and uses the backend. Wait for it before the
    // backend goes and before the library unloads (plugin reload). Drop its result.
    if (job_ && job_->thread_.joinable())
        job_->thread_.join();
    job_.reset();
    compileQueue_.clear();
    // The engine factory of a script type points to this subsystem and maybe into a library that unloads.
    UnregisterReflectedTypes();
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
    // The embedded prelude: the EASTL namespace and the Urho3D::SharedPtr and Urho3D::WeakPtr handles.
    options.preludes.emplace_back(kRbfxPreludeName, kRbfxPreludeText);

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
    // A sync compile does not overlap a background compile: they share the backend and the slot state.
    FinishCompile(true);
    compileQueue_.erase(ea::remove(compileQueue_.begin(), compileQueue_.end(), name), compileQueue_.end());
    std::unique_ptr<CompileJob> job = PrepareCompile(name);
    if (!job)
        return false;
    job->built_ = backend_->load(job->script_, job->generation_);
    return ApplyCompile(*job);
}

std::unique_ptr<ScriptSystem::CompileJob> ScriptSystem::PrepareCompile(const ea::string& name)
{
    if (!EnsureBackend())
        return nullptr;

    // Read the stamp before the text. A change between the two reads then starts one more reload.
    auto job = std::make_unique<CompileJob>();
    job->name_ = name;
    job->stamp_ = source_->GetModificationStamp(name);
    ScriptStatus& status = scripts_[name];
    status.name_ = name;
    // The stamp of the text that compiles. The poll then does not queue the same text again.
    status.stamp_ = job->stamp_;
    ea::string text;
    if (!source_->ReadText(name, text))
    {
        ReportError(Format("cannot read the script '{}' from {}", name, source_->GetDescription()));
        status.error_ = lastError_;
        return nullptr;
    }
    job->script_.name = ScriptSourceFileSystem::ToBackendPath(name);
    job->script_.text.assign(text.c_str(), text.size());
    // The generation must be newer than the active generation of the process. No commit runs
    // during a compile, so the generation is still free when the compile ends.
    job->generation_ = hot::active_generation + 1;
    return job;
}

bool ScriptSystem::ApplyCompile(CompileJob& job)
{
    const ea::string& name = job.name_;
    ScriptStatus& status = scripts_[name];
    hot::LoadResult& built = job.built_;
    if (built.module)
        built.module->unit = job.script_.name;

    hot::ReloadResult result;
    // The first module of the loader goes through load(). Each later module is a reload.
    if (loader_->stats().generations.empty())
    {
        if (!built)
        {
            ReportError(Format("cannot compile '{}': {}", name, ToEaString(built.error)));
            status.error_ = lastError_;
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
        status.error_ = lastError_;
        return false;
    }

    // The classes of this unit have the new generation in their dispatch tables.
    status.loaded_ = true;
    status.error_.clear();
    status.generation_ = hot::active_generation;
    status.classes_.clear();
    for (const hot::ClassInfo& info : loader_->stats().classes)
    {
        if (info.generation == hot::active_generation)
            status.classes_.push_back(ToEaString(info.name));
    }
    lastError_.clear();
    URHO3D_LOGINFO("ScriptSystem: loaded '{}' (generation {})", name, hot::active_generation);
    // New script classes become engine types at the swap.
    RegisterReflectedTypes();
    return true;
}

void ScriptSystem::StartNextCompile()
{
    while (!job_ && !compileQueue_.empty())
    {
        const ea::string name = compileQueue_.front();
        compileQueue_.erase(compileQueue_.begin());
        std::unique_ptr<CompileJob> job = PrepareCompile(name);
        if (!job)
            continue;
        URHO3D_LOGINFO("ScriptSystem: compiling '{}'; the old code runs until the compile ends", name);
        CompileJob* raw = job.get();
        hot::Backend* backend = backend_.get();
        // The worker uses only the job and the backend. The backend reads the source through
        // its thread safe file system.
        job->thread_ = std::thread([raw, backend]
        {
            try
            {
                raw->built_ = backend->load(raw->script_, raw->generation_);
            }
            catch (const std::exception& error)
            {
                raw->built_ = hot::LoadResult{};
                raw->built_.error = error.what();
            }
            raw->done_ = true;
        });
        job_ = std::move(job);
    }
}

void ScriptSystem::FinishCompile(bool wait)
{
    if (!job_ || (!wait && !job_->done_))
        return;
    job_->thread_.join();
    std::unique_ptr<CompileJob> job = std::move(job_);
    // A newer text makes the result stale. Drop it and compile the newest text.
    const unsigned long long current = source_->GetModificationStamp(job->name_);
    if (current != 0 && current != job->stamp_)
    {
        URHO3D_LOGINFO("ScriptSystem: '{}' changed during the compile, compiling again", job->name_);
        if (ea::find(compileQueue_.begin(), compileQueue_.end(), job->name_) == compileQueue_.end())
            compileQueue_.insert(compileQueue_.begin(), job->name_);
        return;
    }
    // On an error the old code stays. The stamp of the failed text prevents a compile at each poll.
    ApplyCompile(*job);
}

void ScriptSystem::RegisterReflectedTypes()
{
    // TODO: Unregister removed classes. The engine can still hold objects of them.
    // TODO: Register the engine attributes of new fields on reload.
    // TODO: Register the fields or properties with an [Attribute] tag as engine attributes.
    for (const hot::ClassInfo& info : loader_->stats().classes)
    {
        if (info.removed
            || ea::find(info.attributes.begin(), info.attributes.end(), ReflectedObjectAttribute) == info.attributes.end())
            continue;
        const ea::string name = ToEaString(info.name);
        const StringHash type(name);
        if (reflectedTypes_.count(type) != 0)
            continue;
        if (context_->GetReflection(type))
        {
            URHO3D_LOGWARNING("ScriptSystem: cannot register '{}': the engine has a type with this name", name);
            continue;
        }
        const hot::ClassEntry* entry = loader_->find(info.name.c_str());
        const ea::string baseName = entry ? EngineBaseName(entry->record->base_name) : ea::string{};
        ObjectReflection* baseReflection = context_->GetReflection(StringHash(baseName));
        if (!baseReflection)
        {
            URHO3D_LOGWARNING("ScriptSystem: cannot register '{}': the base '{}' is not an engine type", name, baseName);
            continue;
        }
        // The prelude puts URHO3D_OBJECT in each shell. The engine registers the static TypeInfo of
        // the shell, so the type of an object and the registered type are the same TypeInfo. The
        // TypeInfo is in the core module of the class, and the core module stays loaded until the
        // loader is destroyed. The destructor unregisters the types before.
        const TypeInfo* typeInfo = ShellTypeInfo(name);
        ObjectReflection* reflection = nullptr;
        if (typeInfo)
            reflection = context_->Reflect(typeInfo);
        else
        {
            URHO3D_LOGWARNING("ScriptSystem: '{}' does not report its own type: a scene saves the base type", name);
            reflection
                = context_->ReflectCustomType(ea::make_unique<TypeInfo>(name.c_str(), baseReflection->GetTypeInfo()));
        }
        reflection->SetObjectFactory(&CreateReflectedObject);
        reflection->CopyAttributesFrom(baseReflection);
        // The editor lists a component type in its menus only when the type has a category under
        // "Component/". SetReflectionCategory also updates the category lists of the context.
        if (baseReflection->GetTypeInfo()->IsTypeOf<Component>())
            context_->SetReflectionCategory(reflection->GetTypeInfo(), Category_User);
        reflectedTypes_.insert(type);
        URHO3D_LOGINFO("ScriptSystem: registered '{}' with the engine (base '{}')", name, baseName);
    }
}

const TypeInfo* ScriptSystem::ShellTypeInfo(const ea::string& className)
{
    // A shell has no static accessor that the host can call, so a temporary object gives the
    // TypeInfo. The script constructor runs one time for this object.
    const hot::ClassEntry* entry = loader_ ? loader_->find(className.c_str()) : nullptr;
    if (!entry || !entry->create)
        return nullptr;
    SharedPtr<Object> object(static_cast<Object*>(entry->create().native));
    if (!object)
        return nullptr;
    const TypeInfo* typeInfo = object->GetTypeInfo();
    return typeInfo && typeInfo->GetTypeName() == className ? typeInfo : nullptr;
}

void ScriptSystem::UnregisterReflectedTypes()
{
    // RemoveReflection also removes the type from its category list.
    if (!context_)
    {
        reflectedTypes_.clear();
        return;
    }
    for (const StringHash type : reflectedTypes_)
    {
        if (ObjectReflection* reflection = context_->GetReflection(type))
        {
            URHO3D_LOGINFO("ScriptSystem: unregistered '{}' from the engine", reflection->GetTypeName());
            context_->RemoveReflection(type);
        }
    }
    reflectedTypes_.clear();
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
    // Load in the order of the names. A file system scan has no fixed order.
    ea::vector<ea::string> names = source_->ListScripts();
    ea::sort(names.begin(), names.end());
    unsigned count = 0;
    for (const ea::string& name : names)
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
    if (!source_)
        return;
    // A script with a compile error is polled too: a fixed file then loads.
    ea::vector<ea::string> changed;
    for (const auto& [name, status] : scripts_)
    {
        const unsigned long long current = source_->GetModificationStamp(name);
        if (current != 0 && current != status.stamp_)
            changed.push_back(name);
    }
    for (const ea::string& name : changed)
    {
        // The running compile of the script finds the change when it ends.
        if ((job_ && job_->name_ == name)
            || ea::find(compileQueue_.begin(), compileQueue_.end(), name) != compileQueue_.end())
            continue;
        URHO3D_LOGINFO("ScriptSystem: '{}' changed, reloading", name);
        compileQueue_.push_back(name);
    }
}

ea::vector<ScriptStatus> ScriptSystem::GetScriptStatus() const
{
    ea::vector<ScriptStatus> result;
    for (const auto& [name, status] : scripts_)
    {
        result.push_back(status);
        result.back().compiling_ = (job_ && job_->name_ == name)
            || ea::find(compileQueue_.begin(), compileQueue_.end(), name) != compileQueue_.end();
    }
    ea::sort(result.begin(), result.end(),
        [](const ScriptStatus& lhs, const ScriptStatus& rhs) { return lhs.name_ < rhs.name_; });
    return result;
}

unsigned ScriptSystem::GetActiveGeneration() const
{
    return hot::active_generation;
}

bool ScriptSystem::IsCompiling() const
{
    return job_ || !compileQueue_.empty();
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
    // A finished background compile goes live now, at the end of the frame.
    FinishCompile(false);
    // A reload that an earlier call left pending goes live now. A commit does not overlap a compile.
    if (!job_ && loader_->reload_pending())
    {
        const hot::ReloadResult result = loader_->commit_pending();
        if (result.status != hot::ReloadStatus::Applied)
            ReportError(Format("cannot commit a pending reload: {}", FormatResult(result)));
    }
    // The next compile starts after the commit, so it reads the new slot state.
    StartNextCompile();
    loader_->frame_boundary();
}

void ScriptSystem::HandleEndFrame(StringHash eventType, VariantMap& eventData)
{
    Update();
}

} // namespace Urho3D

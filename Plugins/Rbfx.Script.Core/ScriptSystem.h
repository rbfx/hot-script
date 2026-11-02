// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include "Rbfx.Script.Core/ScriptSource.h"

#include <Urho3D/Container/Ptr.h>
#include <Urho3D/Core/Object.h>
#include <Urho3D/Core/Timer.h>

#include <EASTL/unordered_map.h>

#include <memory>

namespace hot
{
class Backend;
class FileSystem;
class Loader;
} // namespace hot

namespace Urho3D
{

/// The script subsystem. It owns the Hot++ backend and the Hot++ loader.
///
/// Each script is one Hot++ unit, and the unit name is the script name. The first script goes
/// through Loader::load. Each later script and each reload goes through Loader::request_reload
/// and Loader::commit_pending.
///
/// Threads and safe points: call LoadScript, ReloadScript and LoadAllScripts on the main thread,
/// and never from script code, because they commit the new code at once. The automatic reload
/// runs in the handler of E_ENDFRAME. No script code runs at that time.
///
/// Hot reload: the subsystem polls GetModificationStamp of each loaded script. The interval is
/// SetPollInterval. A changed stamp compiles the script again and commits it at the end of the frame.
/// A change in a header that a script includes does not start a reload. Call ReloadScript for it.
///
/// Lifetime: release all script objects before the subsystem is destroyed. The destructor
/// unloads the script code, and a live script object then has no code.
///
/// One runtime: the operations are virtual. A module that has only this header calls the code
/// of the module that made the subsystem. Then the process has one Hot++ runtime state. Do not
/// link Rbfx.Script.Core into a module only to call the subsystem of another module.
class ScriptSystem : public Object
{
    URHO3D_OBJECT(ScriptSystem, Object);

public:
    /// Construct with the source of the scripts. The source must not be null.
    ScriptSystem(Context* context, std::shared_ptr<IScriptSource> source);
    ~ScriptSystem() override;

    /// Compile a script and make its classes live. Return false on an error.
    /// A script that is loaded already is compiled again.
    virtual bool LoadScript(const ea::string& name);
    /// Compile a loaded script again and replace its code. The live objects keep their fields.
    virtual bool ReloadScript(const ea::string& name);
    /// Load all scripts that the source lists. Return the number of scripts that load.
    virtual unsigned LoadAllScripts();
    /// Return true when the script loaded successfully at least once.
    bool IsScriptLoaded(const ea::string& name) const { return loaded_.find(name) != loaded_.end(); }

    /// Create an object of a script class. Return null when the class is not live.
    /// The pointer points to the native base of the class (the first base of the script class).
    /// The caller owns the object.
    virtual void* CreateNative(const ea::string& className);
    /// Create an object of a script class with the native base T, for example LogicComponent.
    /// T must be the first native base of the script class, or a base of it at offset zero.
    template <class T> SharedPtr<T> CreateObject(const ea::string& className)
    {
        return SharedPtr<T>(static_cast<T*>(CreateNative(className)));
    }

    /// Poll the scripts, commit a pending reload and let Hot++ unload old code.
    /// The handler of E_ENDFRAME calls it.
    virtual void Update();

    /// Enable or disable the automatic reload of changed scripts. The default is enabled.
    void SetAutoReload(bool enable) { autoReload_ = enable; }
    bool GetAutoReload() const { return autoReload_; }
    /// Set the poll interval of the automatic reload in milliseconds. The default is 500.
    void SetPollInterval(unsigned milliseconds) { pollInterval_ = milliseconds; }
    unsigned GetPollInterval() const { return pollInterval_; }

    /// Add an include folder for the script compile. Call it before the first load.
    virtual void AddIncludeDir(const ea::string& path);
    /// Add a compiler flag for the script compile. Call it before the first load.
    virtual void AddCompilerFlag(const ea::string& flag);

    /// Return the source of the scripts.
    IScriptSource* GetSource() const { return source_.get(); }
    /// Return the text of the last error, or an empty string.
    const ea::string& GetLastError() const { return lastError_; }

private:
    /// Make the backend and the loader. Return false on an error.
    bool EnsureBackend();
    /// Compile one script and make it live. The function commits at once.
    bool CompileAndApply(const ea::string& name);
    /// Poll the stamps and reload the changed scripts.
    void PollScripts();
    /// Log an error and keep it as the last error.
    void ReportError(const ea::string& message);
    void HandleEndFrame(StringHash eventType, VariantMap& eventData);

    std::shared_ptr<IScriptSource> source_;
    std::shared_ptr<hot::FileSystem> fileSystem_;
    /// The backend is declared before the loader, so the loader is destroyed first.
    std::unique_ptr<hot::Backend> backend_;
    std::unique_ptr<hot::Loader> loader_;
    /// True after a failed backend creation. The subsystem does not try again.
    bool backendFailed_{};
    /// The loaded scripts and the stamp of the last compile.
    ea::unordered_map<ea::string, unsigned long long> loaded_;
    ea::vector<ea::string> extraIncludeDirs_;
    ea::vector<ea::string> extraFlags_;
    bool autoReload_{true};
    unsigned pollInterval_{500};
    Timer pollTimer_;
    ea::string lastError_;
    /// The handle that makes the symbols of this module global (see ScriptSystem.cpp).
    void* selfHandle_{};
};

} // namespace Urho3D

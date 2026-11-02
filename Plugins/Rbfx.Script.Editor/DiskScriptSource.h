// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include <Rbfx.Script.Core/ScriptSource.h>

#include <mutex>

namespace Urho3D
{

/// The script source of the editor: one folder on the disk. It does not use the ResourceCache.
/// A name is relative to the folder, for example "Rotator.hot" or "Ai/Brain.hot".
/// All functions are thread safe.
class DiskScriptSource : public IScriptSource
{
public:
    explicit DiskScriptSource(const ea::string& directory);

    /// Set the folder. The script system reads the new folder at the next load.
    void SetDirectory(const ea::string& directory);
    /// Return the folder.
    ea::string GetDirectory() const;

    /// Implement IScriptSource.
    /// @{
    ea::vector<ea::string> ListScripts() override;
    bool ReadText(const ea::string& name, ea::string& text) override;
    bool Exists(const ea::string& name) override;
    unsigned long long GetModificationStamp(const ea::string& name) override;
    ea::string GetDescription() const override;
    /// @}

private:
    /// Return the disk path of a name, or an empty string for a name that is not safe.
    ea::string ToDiskPath(const ea::string& name) const;

    mutable std::mutex mutex_;
    ea::string directory_;
};

} // namespace Urho3D

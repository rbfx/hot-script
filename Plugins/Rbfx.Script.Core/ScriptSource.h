// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include "Rbfx.Script.Core/ScriptCoreApi.h"

#include <Urho3D/Urho3D.h>

#include <EASTL/string.h>
#include <EASTL/vector.h>

namespace Urho3D
{

/// The origin of the script files. The script system reads all scripts and headers through it.
/// The game plugin reads from the ResourceCache. The editor plugin reads from a disk folder.
/// A name is a relative path with "/" between the parts, for example "Scripts/Rotator.hot".
/// The Hot++ backend can call ReadText, Exists and GetModificationStamp from more than one
/// thread at the same time, so an implementation must be thread safe.
class RBFX_SCRIPT_CORE_API IScriptSource
{
public:
    virtual ~IScriptSource() = default;

    /// Return the names of all scripts (".hot" and ".cpp" files). Headers are not scripts.
    /// The list includes the user preludes ("*.prelude.hot"). The script system does not compile them as scripts.
    virtual ea::vector<ea::string> ListScripts() = 0;
    /// Read the text of a script or of a header. Return false when the file does not exist.
    virtual bool ReadText(const ea::string& name, ea::string& text) = 0;
    /// Return true when the file exists.
    virtual bool Exists(const ea::string& name) = 0;
    /// Return a number that changes when the file changes. Zero means "unknown".
    /// The script system does not poll a file with the stamp zero.
    virtual unsigned long long GetModificationStamp(const ea::string& name) = 0;
    /// Return a short text for the log, for example "ResourceCache:Scripts".
    virtual ea::string GetDescription() const = 0;
};

/// Return true when the name has the suffix ".hot" or ".cpp".
RBFX_SCRIPT_CORE_API bool IsScriptFileName(const ea::string& name);
/// Return true when the name has the suffix ".prelude.hot": a user prelude, not a script.
RBFX_SCRIPT_CORE_API bool IsPreludeFileName(const ea::string& name);

} // namespace Urho3D

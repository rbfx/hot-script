// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "ResourceScriptSource.h"

#include <Urho3D/Core/Context.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/ScanFlags.h>
#include <Urho3D/Resource/ResourceCache.h>

#include <EASTL/sort.h>

namespace Urho3D
{

ResourceScriptSource::ResourceScriptSource(Context* context, const ea::string& resourceDir)
    : context_(context)
    , resourceDir_(resourceDir)
{
}

ea::vector<ea::string> ResourceScriptSource::ListScripts()
{
    ea::vector<ea::string> result;
    auto* cache = context_ ? context_->GetSubsystem<ResourceCache>() : nullptr;
    if (!cache)
        return result;

    ea::vector<ea::string> files;
    cache->Scan(files, resourceDir_, "*.*", SCAN_FILES | SCAN_RECURSIVE);
    for (const ea::string& file : files)
    {
        const ea::string name = resourceDir_ + "/" + file;
        if (IsScriptFileName(name))
            result.push_back(name);
    }
    // Two resource folders can have the same file. Keep one name, in a fixed order.
    ea::sort(result.begin(), result.end());
    result.erase(ea::unique(result.begin(), result.end()), result.end());
    return result;
}

bool ResourceScriptSource::ReadText(const ea::string& name, ea::string& text)
{
    auto* cache = context_ ? context_->GetSubsystem<ResourceCache>() : nullptr;
    if (!cache)
        return false;
    // ResourceCache::GetFile is safe outside the main thread.
    AbstractFilePtr file = cache->GetFile(name, false);
    if (!file)
        return false;
    text.resize(file->GetSize());
    if (!text.empty() && file->Read(text.data(), text.size()) != text.size())
        return false;
    return true;
}

bool ResourceScriptSource::Exists(const ea::string& name)
{
    auto* cache = context_ ? context_->GetSubsystem<ResourceCache>() : nullptr;
    return cache && cache->Exists(name);
}

unsigned long long ResourceScriptSource::GetModificationStamp(const ea::string& name)
{
    auto* cache = context_ ? context_->GetSubsystem<ResourceCache>() : nullptr;
    auto* fileSystem = context_ ? context_->GetSubsystem<FileSystem>() : nullptr;
    if (!cache || !fileSystem)
        return 0;
    const ea::string path = cache->GetResourceFileName(name);
    if (path.empty())
        return 0;
    return static_cast<unsigned long long>(fileSystem->GetLastModifiedTime(path));
}

ea::string ResourceScriptSource::GetDescription() const
{
    return "ResourceCache:" + resourceDir_;
}

} // namespace Urho3D

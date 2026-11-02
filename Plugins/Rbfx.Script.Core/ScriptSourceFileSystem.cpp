// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "Rbfx.Script.Core/ScriptSourceFileSystem.h"

#include <cstring>

namespace Urho3D
{

bool IsScriptFileName(const ea::string& name)
{
    return name.ends_with(".hot") || name.ends_with(".cpp");
}

bool IsPreludeFileName(const ea::string& name)
{
    return name.ends_with(".prelude.hot");
}

ScriptSourceFileSystem::ScriptSourceFileSystem(std::shared_ptr<IScriptSource> source)
    : source_(std::move(source))
{
}

std::string ScriptSourceFileSystem::ToBackendPath(const ea::string& name)
{
    std::string path = VirtualRoot;
    path += '/';
    path.append(name.c_str(), name.size());
    return path;
}

ea::string ScriptSourceFileSystem::ToSourceName(const std::string& path)
{
    const size_t rootLength = std::strlen(VirtualRoot);
    if (path.size() <= rootLength + 1 || path.compare(0, rootLength, VirtualRoot) != 0 || path[rootLength] != '/')
        return {};

    // Split the rest into parts and remove "." and "..". A ".." above the root is not valid.
    ea::vector<ea::string> parts;
    size_t pos = rootLength + 1;
    while (pos <= path.size())
    {
        size_t end = path.find('/', pos);
        if (end == std::string::npos)
            end = path.size();
        const ea::string part(path.data() + pos, end - pos);
        pos = end + 1;
        if (part.empty() || part == ".")
            continue;
        if (part == "..")
        {
            if (parts.empty())
                return {};
            parts.pop_back();
            continue;
        }
        parts.push_back(part);
    }

    ea::string name;
    for (const ea::string& part : parts)
    {
        if (!name.empty())
            name += '/';
        name += part;
    }
    return name;
}

bool ScriptSourceFileSystem::read(const std::string& path, std::string& out)
{
    const ea::string name = ToSourceName(path);
    if (name.empty())
        return false;
    ea::string text;
    if (!source_->ReadText(name, text))
        return false;
    out.assign(text.c_str(), text.size());
    return true;
}

hot::VfsStat ScriptSourceFileSystem::stat(const std::string& path)
{
    const ea::string name = ToSourceName(path);
    if (name.empty() || !source_->Exists(name))
        return {};
    return {true, source_->GetModificationStamp(name)};
}

} // namespace Urho3D

// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#include "DiskScriptSource.h"

#include <EASTL/sort.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace Urho3D
{

namespace fs = std::filesystem;

DiskScriptSource::DiskScriptSource(const ea::string& directory)
    : directory_(directory)
{
}

void DiskScriptSource::SetDirectory(const ea::string& directory)
{
    std::lock_guard<std::mutex> lock(mutex_);
    directory_ = directory;
}

ea::string DiskScriptSource::GetDirectory() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return directory_;
}

ea::string DiskScriptSource::ToDiskPath(const ea::string& name) const
{
    // A name must stay in the folder: no absolute path and no ".." part.
    if (name.empty() || name.front() == '/' || name.find(':') != ea::string::npos)
        return {};
    for (const fs::path& part : fs::path(name.c_str()))
    {
        if (part == "..")
            return {};
    }
    const ea::string directory = GetDirectory();
    if (directory.empty())
        return {};
    return (fs::path(directory.c_str()) / name.c_str()).string().c_str();
}

ea::vector<ea::string> DiskScriptSource::ListScripts()
{
    ea::vector<ea::string> result;
    const fs::path root(GetDirectory().c_str());
    std::error_code error;
    if (root.empty() || !fs::is_directory(root, error))
        return result;
    for (fs::recursive_directory_iterator iter(root, error), end; !error && iter != end; iter.increment(error))
    {
        if (!iter->is_regular_file(error))
            continue;
        const ea::string name = iter->path().lexically_relative(root).generic_string().c_str();
        if (IsScriptFileName(name))
            result.push_back(name);
    }
    ea::sort(result.begin(), result.end());
    return result;
}

bool DiskScriptSource::ReadText(const ea::string& name, ea::string& text)
{
    const ea::string path = ToDiskPath(name);
    if (path.empty())
        return false;
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file)
        return false;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();
    text.assign(content.c_str(), content.size());
    return true;
}

bool DiskScriptSource::Exists(const ea::string& name)
{
    const ea::string path = ToDiskPath(name);
    std::error_code error;
    return !path.empty() && fs::is_regular_file(path.c_str(), error);
}

unsigned long long DiskScriptSource::GetModificationStamp(const ea::string& name)
{
    const ea::string path = ToDiskPath(name);
    if (path.empty())
        return 0;
    std::error_code error;
    const auto time = fs::last_write_time(path.c_str(), error);
    if (error)
        return 0;
    const auto ticks = static_cast<unsigned long long>(time.time_since_epoch().count());
    return ticks != 0 ? ticks : 1;
}

ea::string DiskScriptSource::GetDescription() const
{
    return "Disk:" + GetDirectory();
}

} // namespace Urho3D

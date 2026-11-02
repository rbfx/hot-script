// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

// The end-to-end test of the game plugin. It opens the plugin library as the engine does
// (dlopen without RTLD_GLOBAL), loads the scripts of <data-dir>/Scripts, creates the script
// components, updates them, changes a script, reloads it and updates again.
//   Rbfx.Script.SmokeTest <plugin-library> <data-dir>
// The test input is Project/Data, the data of the editor demo. Give a writable copy of it: the test changes
// Scripts/Rotator2.cpp and restores it.

#include <Rbfx.Script.Core/ScriptSystem.h>

#include <Urho3D/Core/Context.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Plugins/Plugin.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Scene.h>

#include <dlfcn.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace Urho3D;

namespace
{

int Fail(const char* message)
{
    std::fprintf(stderr, "SmokeTest FAILED: %s\n", message);
    return 1;
}

bool ReadFile(const std::string& path, std::string& text)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    return true;
}

bool WriteFile(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return static_cast<bool>(out);
}

float YawOf(Node* node)
{
    return node->GetRotation().YawAngle();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: Rbfx.Script.SmokeTest <plugin-library> <data-dir>\n");
        return 2;
    }
    const std::string pluginPath = argv[1];
    const std::string dataDir = argv[2];

    SharedPtr<Context> context(new Context());
    SharedPtr<Engine> engine(new Engine(context));
    StringVariantMap parameters;
    parameters[EP_HEADLESS] = true;
    // The engine mounts a resource folder relative to a prefix folder.
    const ea::string dataPath = RemoveTrailingSlash(ea::string(dataDir.c_str()));
    parameters[EP_RESOURCE_PREFIX_PATHS] = GetParentPath(dataPath);
    parameters[EP_RESOURCE_PATHS] = GetFileName(dataPath);
    parameters[EP_AUTOLOAD_PATHS] = "";
    parameters[EP_LOG_NAME] = "";
    if (!engine->Initialize(parameters, {}))
        return Fail("engine initialization");

    void* library = dlopen(pluginPath.c_str(), RTLD_NOW);
    if (!library)
        return Fail(dlerror());
    using PluginMain = Plugin* (*)(Context*);
    auto pluginMain = reinterpret_cast<PluginMain>(dlsym(library, "PluginDynamicLibraryMain"));
    if (!pluginMain)
        return Fail("no PluginDynamicLibraryMain");

    int result = 0;
    {
        SharedPtr<Plugin> plugin(pluginMain(context));
        plugin->LoadPlugin();

        auto* system = static_cast<ScriptSystem*>(context->GetSubsystem(ScriptSystem::GetTypeStatic()));
        if (!system)
            return Fail("no ScriptSystem subsystem");
        if (!system->IsScriptLoaded("Scripts/Rotator.hot"))
            result = Fail("Rotator.hot did not load");
        if (!system->IsScriptLoaded("Scripts/Rotator2.cpp"))
            result = Fail("Rotator2.cpp did not load");

        auto scene = MakeShared<Scene>(context);
        for (const char* className : {"Rotator", "Rotator2"})
        {
            Node* node = scene->CreateChild(className);
            SharedPtr<LogicComponent> component = system->CreateObject<LogicComponent>(className);
            if (!component)
            {
                result = Fail("CreateObject returned null");
                continue;
            }
            node->AddComponent(component, 0);
            component->Update(0.5f);
            std::printf("SmokeTest: %s yaw after 0.5 s = %.1f\n", className, YawOf(node));
            if (YawOf(node) < 40.0f || YawOf(node) > 50.0f)
                result = Fail("unexpected yaw");
        }

        // Hot reload: change the speed of Rotator2 and reload. The live component keeps its node.
        Node* node2 = scene->GetChild("Rotator2");
        auto* rotator2 = node2 ? node2->GetComponent<LogicComponent>() : nullptr;
        const std::string path2 = dataDir + "/Scripts/Rotator2.cpp";
        std::string original;
        if (rotator2 && ReadFile(path2, original))
        {
            std::string changed = original;
            const size_t pos = changed.find("speed * timeStep");
            if (pos != std::string::npos)
                changed.replace(pos, 16, "-speed * timeStep");
            WriteFile(path2, changed);
            const bool reloaded = system->ReloadScript("Scripts/Rotator2.cpp");
            WriteFile(path2, original);
            if (!reloaded)
                result = Fail("ReloadScript failed");
            rotator2->Update(0.5f);
            std::printf("SmokeTest: Rotator2 yaw after reload = %.1f\n", YawOf(node2));
            if (YawOf(node2) < -5.0f || YawOf(node2) > 5.0f)
                result = Fail("the reload did not change the code");
        }

        scene.Reset();
        plugin->UnloadPlugin();
        plugin.Reset();
    }
    engine.Reset();
    context.Reset();
    std::printf(result == 0 ? "SmokeTest PASSED\n" : "SmokeTest FAILED\n");
    return result;
}

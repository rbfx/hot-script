// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

// The end-to-end test of the game plugin. It opens the plugin library as the engine does
// (dlopen without RTLD_GLOBAL), loads the scripts of <data-dir>/Scripts, creates the script
// components, updates them, changes a script, reloads it and updates again.
// Then it unloads the plugin, loads a second plugin instance and checks the engine types again.
//   Rbfx.Script.SmokeTest <plugin-library> <data-dir>
// The test input is Project/Data, the data of the editor demo. Give a writable copy of it: the test changes
// Scripts/Rotator2.cpp and restores it.
// The background reload part checks that the old code runs during a compile of a changed script, that the
// new code runs after the swap, and that a compile error keeps the old code. A compile takes seconds,
// so many frames run during it.

#include <Rbfx.Script.Core/ScriptSystem.h>

#include <Urho3D/Core/Context.h>
#include <Urho3D/IO/IOEvents.h>
#include <Urho3D/Core/ObjectCategory.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/Plugins/Plugin.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Scene/Node.h>
#include <Urho3D/Scene/Scene.h>

#include <dlfcn.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

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

// Save a scene with a component of the type and load it into a new scene. The result is the type name
// of the loaded component, or an empty string.
ea::string SceneRoundTrip(Context* context, const char* className)
{
    auto scene = MakeShared<Scene>(context);
    scene->CreateChild("Saved")->CreateComponent(StringHash(className));
    VectorBuffer saved;
    if (!scene->SaveXML(saved))
        return {};
    auto loadedScene = MakeShared<Scene>(context);
    MemoryBuffer savedData(saved.GetBuffer());
    if (!loadedScene->LoadXML(savedData))
        return {};
    Node* node = loadedScene->GetChild("Saved");
    Component* loaded = node && !node->GetComponents().empty() ? node->GetComponents()[0].Get() : nullptr;
    std::printf("SmokeTest: the loaded scene has the component '%s'\n", loaded ? loaded->GetTypeName().c_str() : "(none)");
    return loaded ? loaded->GetTypeName() : ea::string{};
}

// A script object reports its script type, and the engine registers the same TypeInfo.
bool ReportsRegisteredType(Context* context, Object* object, const char* className)
{
    ObjectReflection* reflection = context->GetReflection(StringHash(className));
    return object && reflection && object->GetTypeName() == className && object->GetType() == StringHash(className)
        && object->GetTypeInfo() == reflection->GetTypeInfo() && object->IsInstanceOf<LogicComponent>();
}

float YawOf(Node* node)
{
    return node->GetRotation().YawAngle();
}

/// Count the log warnings and errors of the ScriptSystem.
class LogCounter : public Object
{
    URHO3D_OBJECT(LogCounter, Object);

public:
    explicit LogCounter(Context* context)
        : Object(context)
    {
        SubscribeToEvent(E_LOGMESSAGE,
            [this](VariantMap& eventData)
            {
                const int level = eventData[LogMessage::P_LEVEL].GetInt();
                const ea::string& message = eventData[LogMessage::P_MESSAGE].GetString();
                if (level >= LOG_WARNING && message.contains("ScriptSystem"))
                    ++count_;
            });
    }

    unsigned count_{};
};

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

        // Reflected script types: Rotator has the attribute ReflectedObject, and Rotator2
        // (a .cpp script) has RBFX_SCRIPT_REFLECTED. So the engine knows them and makes them by name.
        const auto& categories = context->GetObjectCategories();
        const auto user = categories.find(Category_User.c_str());
        for (const char* className : {"Rotator", "Rotator2"})
        {
            const StringHash type(className);
            ObjectReflection* reflection = context->GetReflection(type);
            if (!reflection)
            {
                std::fprintf(stderr, "SmokeTest: %s is not registered with the engine\n", className);
                result = Fail("a tagged script class is not registered");
                continue;
            }
            const TypeInfo* base = reflection->GetTypeInfo()->GetBaseTypeInfo();
            if (!base || base->GetType() != LogicComponent::GetTypeStatic())
                result = Fail("the base of a reflected type is not LogicComponent");
            // The editor menus list the types of the "Component/" categories that have a factory.
            if (!reflection->HasObjectFactory() || reflection->GetCategory() != Category_User || user == categories.end()
                || ea::find(user->second.begin(), user->second.end(), type) == user->second.end())
                result = Fail("a reflected type is not in the category Component/User");
            if (!context->CreateObject(type))
                result = Fail("Context::CreateObject returned null for a reflected type");
            std::printf("SmokeTest: %s is registered with the engine\n", className);
        }
        for (const char* className : {"Rotator", "Rotator2"})
        {
            Node* node = scene->CreateChild(Format("Reflected{}", className));
            auto* component = dynamic_cast<LogicComponent*>(node->CreateComponent(StringHash(className)));
            if (!component)
            {
                result = Fail("Node::CreateComponent did not make a script component");
                continue;
            }
            if (!ReportsRegisteredType(context, component, className))
                result = Fail("a script component does not report its registered type");
            component->Update(0.5f);
            std::printf("SmokeTest: reflected %s yaw after 0.5 s = %.1f\n", className, YawOf(node));
            if (YawOf(node) < 40.0f || YawOf(node) > 50.0f)
                result = Fail("unexpected yaw of a reflected component");
        }
        for (const char* className : {"Rotator", "Rotator2"})
        {
            if (SceneRoundTrip(context, className) != className)
                result = Fail("a scene does not keep the type of a script component");
        }

        // Hot reload: change the speed of Rotator2 and reload. The live component keeps its node and
        // its type. The component reports the type Rotator2, so GetComponent<LogicComponent> does not find it.
        Node* node2 = scene->GetChild("Rotator2");
        auto* rotator2 = node2 ? node2->GetDerivedComponent<LogicComponent>() : nullptr;
        if (!rotator2)
            result = Fail("the Rotator2 node has no script component");
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
            if (!ReportsRegisteredType(context, rotator2, "Rotator2"))
                result = Fail("a script component does not report its registered type after a reload");
            if (SceneRoundTrip(context, "Rotator2") != "Rotator2")
                result = Fail("a scene does not keep the type of a script component after a reload");
        }

        // Background reload: the poll finds the change and compiles on a worker thread. The old code runs
        // until the compile ends. At that time the live code of Rotator2 is "-speed" (-45 degrees in 0.5 s).
        if (rotator2 && !original.empty())
        {
            namespace stdfs = std::filesystem;
            system->SetAutoReload(true);
            system->SetPollInterval(0);
            int stampOffset = 0;
            // The stamp has a resolution of one second, so each write sets a new modification time.
            const auto writeScript = [&](const char* replacement)
            {
                std::string text = original;
                const size_t pos = text.find("speed * timeStep");
                if (pos != std::string::npos)
                    text.replace(pos, 16, replacement);
                WriteFile(path2, text);
                stampOffset += 10;
                std::error_code ec;
                stdfs::last_write_time(path2, stdfs::file_time_type::clock::now() + std::chrono::seconds(stampOffset), ec);
            };
            const auto yawOfOneUpdate = [&]
            {
                node2->SetRotation(Quaternion::IDENTITY);
                rotator2->Update(0.5f);
                return YawOf(node2);
            };
            const auto near = [](float value, float expected) { return value > expected - 5.0f && value < expected + 5.0f; };
            // Run frames while the compile runs. Each frame must run the code with the expected yaw.
            // A second text replaces the first one during the compile, if it is not null.
            const auto runFramesDuringCompile = [&](float expectedYaw, const char* secondText)
            {
                system->Update(); // the poll queues the script and starts the compile
                if (!system->IsCompiling())
                    return Fail("a changed script does not start a background compile");
                int frames = 0;
                const auto start = std::chrono::steady_clock::now();
                while (system->IsCompiling())
                {
                    if (!near(yawOfOneUpdate(), expectedYaw))
                        return Fail("the old code does not run during the background compile");
                    ++frames;
                    if (frames == 1 && secondText)
                        writeScript(secondText);
                    if (std::chrono::steady_clock::now() - start > std::chrono::minutes(3))
                        return Fail("the background compile does not end");
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    system->Update();
                }
                std::printf("SmokeTest: %d frames ran the old code during the background compile\n", frames);
                return frames > 1 ? 0 : Fail("no frame ran during the background compile");
            };
            const auto rotator2Error = [&]
            {
                for (const ScriptStatus& status : system->GetScriptStatus())
                {
                    if (status.name_ == "Scripts/Rotator2.cpp")
                        return status.error_;
                }
                return ea::string("no status");
            };

            // Two saves in a row: the result of the first text is stale, and only the second text goes live.
            const unsigned generation0 = system->GetActiveGeneration();
            writeScript("3.0f * speed * timeStep");
            if (runFramesDuringCompile(-45.0f, "2.0f * speed * timeStep") != 0)
                result = 1;
            const float newYaw = yawOfOneUpdate();
            std::printf("SmokeTest: Rotator2 yaw after the background reload = %.1f\n", newYaw);
            if (!near(newYaw, 90.0f))
                result = Fail("the new code does not run after the background reload");
            if (system->GetActiveGeneration() != generation0 + 1)
                result = Fail("the background reload did not commit exactly one generation");
            if (node2->GetDerivedComponent<LogicComponent>() != rotator2 || !ReportsRegisteredType(context, rotator2, "Rotator2"))
                result = Fail("the script component does not keep its object and type after the background reload");
            if (!rotator2Error().empty())
                result = Fail("the background reload reports an error");

            // User preludes. The test adds a user prelude to the copy of the data folder. The library
            // text stays: the script components above still report their registered types.
            {
                const auto scriptError = [&](const char* name)
                {
                    for (const ScriptStatus& status : system->GetScriptStatus())
                    {
                        if (status.name_ == name)
                            return status.error_;
                    }
                    return ea::string("no status");
                };
                // A prelude change starts a background compile of all scripts. Wait for its end.
                const auto compileAll = [&](const char* what)
                {
                    system->Update();
                    if (!system->IsCompiling())
                        return Fail(what);
                    const auto start = std::chrono::steady_clock::now();
                    while (system->IsCompiling())
                    {
                        if (std::chrono::steady_clock::now() - start > std::chrono::minutes(5))
                            return Fail("the compile after a prelude change does not end");
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        system->Update();
                    }
                    return 0;
                };
                const auto rotatorLive = [&] { return system->CreateObject<LogicComponent>("Rotator") != nullptr; };
                const std::string preludePath = dataDir + "/Scripts/Test.prelude.hot";
                const std::string overridePath = dataDir + "/Scripts/Zz.prelude.hot";
                const std::string preludeText =
                    "[cpp_prelude(\"using ScriptNativeBase = $B;\")]\nclass : Urho3D.Object;\n";
                WriteFile(preludePath, preludeText);
                if (compileAll("a new user prelude does not start a compile") != 0)
                    result = 1;
                if (!scriptError("Scripts/Rotator.hot").empty())
                    result = Fail("Rotator does not compile with the user prelude");

                // A new prelude overrides the storage of the library rule of Urho3D.RefCounted. It keeps the inherited
                // weak handle, so the merged rule is not valid. Rotator then has an error, and the old code stays.
                WriteFile(overridePath, "[native, value] class Urho3D.RefCounted;\n");
                if (compileAll("a new user prelude does not start a compile") != 0)
                    result = 1;
                const ea::string overrideError = scriptError("Scripts/Rotator.hot");
                std::printf("SmokeTest: Rotator with the override prelude: %s\n", overrideError.c_str());
                if (overrideError.empty() || !rotatorLive())
                    result = Fail("the override of a prelude attribute has no effect or removes the old code");

                // The removal of the prelude compiles all scripts again with the library rule.
                std::error_code ec;
                stdfs::remove(overridePath, ec);
                if (compileAll("a removed user prelude does not start a compile") != 0)
                    result = 1;
                if (!scriptError("Scripts/Rotator.hot").empty())
                    result = Fail("Rotator does not compile after the removal of the override prelude");

                // A broken prelude keeps the old code and shows the error in the status of each script.
                const auto writePrelude = [&](const std::string& text)
                {
                    WriteFile(preludePath, text);
                    stampOffset += 10;
                    stdfs::last_write_time(
                        preludePath, stdfs::file_time_type::clock::now() + std::chrono::seconds(stampOffset), ec);
                };
                writePrelude(preludeText + "[broken\n");
                if (compileAll("a changed user prelude does not start a compile") != 0)
                    result = 1;
                const ea::string brokenError = scriptError("Scripts/Rotator.hot");
                if (brokenError.find("Test.prelude.hot") == ea::string::npos || !rotatorLive() || !near(yawOfOneUpdate(), 90.0f))
                    result = Fail("a broken user prelude does not keep the old code or does not name the prelude");

                writePrelude(preludeText);
                if (compileAll("a restored user prelude does not start a compile") != 0)
                    result = 1;
                if (!scriptError("Scripts/Rotator.hot").empty() || !rotator2Error().empty())
                    result = Fail("the scripts do not compile after the user prelude is restored");
                if (!ReportsRegisteredType(context, rotator2, "Rotator2"))
                    result = Fail("the library prelude text is lost after the user prelude changes");

                stdfs::remove(preludePath, ec);
                if (compileAll("a removed user prelude does not start a compile") != 0)
                    result = 1;
            }

            // A compile error keeps the old code.
            const unsigned generation1 = system->GetActiveGeneration();
            writeScript("speed * timeStep +");
            if (runFramesDuringCompile(90.0f, nullptr) != 0)
                result = 1;
            if (!near(yawOfOneUpdate(), 90.0f) || system->GetActiveGeneration() != generation1)
                result = Fail("a compile error does not keep the old code");
            if (rotator2Error().empty())
                result = Fail("a compile error is not in the script status");
            // The plugin unloads during a compile of the original text. The destructor waits for the compile.
            writeScript("speed * timeStep");
            system->Update();
            if (!system->IsCompiling())
                result = Fail("the restored script does not start a background compile");
        }

        scene.Reset();
        plugin->UnloadPlugin();
        plugin.Reset();
    }

    // The unload removes the script types from the engine. A second plugin instance (as after a plugin reload
    // of the editor) then registers them again with no warning.
    for (const char* className : {"Rotator", "Rotator2"})
    {
        if (context->GetReflection(StringHash(className)))
            result = Fail("a script type stays registered after the plugin unloads");
    }
    {
        auto logCounter = MakeShared<LogCounter>(context);
        SharedPtr<Plugin> plugin(pluginMain(context));
        plugin->LoadPlugin();
        if (logCounter->count_ != 0)
            result = Fail("the second plugin instance logs ScriptSystem warnings");
        for (const char* className : {"Rotator", "Rotator2"})
        {
            ObjectReflection* reflection = context->GetReflection(StringHash(className));
            if (!reflection || !reflection->HasObjectFactory())
                result = Fail("the second plugin instance does not register a script type");
        }

        // The second plugin instance registers a new TypeInfo. A scene still keeps the script type.
        for (const char* className : {"Rotator", "Rotator2"})
        {
            SharedPtr<Object> object = context->CreateObject(StringHash(className));
            if (!ReportsRegisteredType(context, object, className))
                result = Fail("a script component does not report its registered type in the second plugin instance");
        }
        for (const char* className : {"Rotator", "Rotator2"})
        {
            if (SceneRoundTrip(context, className) != className)
                result = Fail("a scene does not keep the type of a script component in the second plugin instance");
        }
        plugin->UnloadPlugin();
        plugin.Reset();
    }
    engine.Reset();
    context.Reset();
    std::printf(result == 0 ? "SmokeTest PASSED\n" : "SmokeTest FAILED\n");
    return result;
}

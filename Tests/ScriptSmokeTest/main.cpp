// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

// The end-to-end test of the game plugin. It opens the plugin library as the engine does
// (dlopen without RTLD_GLOBAL), loads the scripts of <data-dir>/Scripts, creates the script
// components, updates them, changes a script, reloads it and updates again.
// Then it unloads the plugin, loads a second plugin instance and checks the engine types again.
//   Rbfx.Script.SmokeTest <plugin-library> <data-dir>
// The test input is Project/Data, the data of the editor demo. Give a writable copy of it: the test changes
// Scripts/Rotator2.cpp and restores it.

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

        // The prelude: Spawner holds a node through Urho3D::SharedPtr and watches it through
        // Urho3D::WeakPtr. The weak handle gives null after the node is removed.
        if (!system->IsScriptLoaded("Scripts/Spawner.hot"))
            result = Fail("Spawner.hot did not load");
        else if (SharedPtr<LogicComponent> spawner = system->CreateObject<LogicComponent>("Spawner"))
        {
            Node* node = scene->CreateChild("Spawner");
            node->AddComponent(spawner, 0);
            spawner->Update(0.1f);
            Node* spawned = node->GetChild("Spawned");
            if (!spawned)
                result = Fail("Spawner did not make the node");
            else if (spawned->Refs() != 1)
                result = Fail("the script keeps a strong reference to the node");
            spawner->Update(0.1f);
            if (node->GetName() != "Spawner")
                result = Fail("the weak handle is null while the node is alive");
            if (spawned)
                spawned->Remove();
            spawner->Update(0.1f);
            std::printf("SmokeTest: Spawner node name after the remove = %s\n", node->GetName().c_str());
            if (node->GetName() != "SpawnerDone")
                result = Fail("the weak handle is not null after the node is removed");
        }
        else
            result = Fail("CreateObject(Spawner) returned null");

        // Reflected script types: Rotator and Spawner have the attribute ReflectedObject, and Rotator2
        // (a .cpp script) has RBFX_SCRIPT_REFLECTED. So the engine knows them and makes them by name.
        // Counter has no attribute: the engine does not know it.
        const auto& categories = context->GetObjectCategories();
        const auto user = categories.find(Category_User.c_str());
        for (const char* className : {"Rotator", "Rotator2", "Spawner"})
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
        if (context->GetReflection(StringHash("Counter")))
            result = Fail("Counter is registered with the engine, but it has no attribute");
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

        scene.Reset();
        plugin->UnloadPlugin();
        plugin.Reset();
    }

    // The unload removes the script types from the engine. A second plugin instance (as after a plugin reload
    // of the editor) then registers them again with no warning.
    for (const char* className : {"Rotator", "Rotator2", "Spawner"})
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
        for (const char* className : {"Rotator", "Rotator2", "Spawner"})
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

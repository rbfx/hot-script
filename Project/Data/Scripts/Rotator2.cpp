// Test input of the smoke test. Rotator2: the same component as Rotator.hot, as a C++ script.
// The factory of a script class takes no arguments, so the constructor gets the context
// from Context::GetInstance.
#include <Urho3D/Core/Context.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Scene/Node.h>
#include <Rbfx.Script.Core/ScriptReflection.h>

// RBFX_SCRIPT_REFLECTED registers Rotator2 with the engine, the same as "[ReflectedObject]" in Rotator.hot.
class RBFX_SCRIPT_REFLECTED Rotator2 : public Urho3D::LogicComponent
{
public:
    Rotator2() : Urho3D::LogicComponent(Urho3D::Context::GetInstance()) {}

    void Update(float timeStep) override
    {
        if (Urho3D::Node* node = GetNode())
            node->Rotate(Urho3D::Quaternion(0.0f, speed * timeStep, 0.0f), Urho3D::TS_LOCAL);
    }

    float speed = 90.0f;
};

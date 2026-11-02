#include <Urho3D/Core/Context.h>
#include <Urho3D/Scene/LogicComponent.h>
#include <Urho3D/Scene/Node.h>

#include <Rbfx.Script.Core/ScriptReflection.h>

using namespace Urho3D;

class RBFX_SCRIPT_REFLECTED Rotator2 : public LogicComponent
{
public:
    URHO3D_OBJECT(Rotator2, LogicComponent);

    explicit Rotator2(Context* context) : LogicComponent(context) {}

    void Update(float timeStep) override
    {
        if (Node* node = GetNode())
            node->Rotate(Quaternion(0.0f, 0.0f, speed * timeStep), TS_LOCAL);
    }

    float speed = 90.0f;
};

// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

// The registration of script classes with the engine. A .hot script writes "[ReflectedObject]"
// before the class. A .cpp script includes this header and writes RBFX_SCRIPT_REFLECTED after
// "class":
//
//     #include <Rbfx.Script.Core/ScriptReflection.h>
//     class RBFX_SCRIPT_REFLECTED Rotator2 : public Urho3D::LogicComponent { ... };
//
// The rbfx prelude declares the Hot++ attribute class. After each script load, the ScriptSystem
// registers each class with the attribute (ScriptSystem::RegisterReflectedTypes).

#include <hot/script.h>

/// The name of the Hot++ script attribute that registers a script class with the engine.
#define RBFX_SCRIPT_REFLECTED_ATTRIBUTE "ReflectedObject"

/// Put it after "class" in a .cpp script. It is the same as "[ReflectedObject]" in a .hot script.
/// It also tells Hot++ that URHO3D_OBJECT in the class body is for the shell. The shell then reports
/// the script type, and the script has no slot for the four functions that the macro overrides.
/// The file stays a normal C++ file: URHO3D_OBJECT stays in the class.
#define RBFX_SCRIPT_REFLECTED \
    HOT_ATTRIBUTE(RBFX_SCRIPT_REFLECTED_ATTRIBUTE) \
    HOT_SHELL_MACRO("URHO3D_OBJECT") \
    HOT_NO_SLOT("GetType GetTypeName GetTypeInfo IsInstanceOf")

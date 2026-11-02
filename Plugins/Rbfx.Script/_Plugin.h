// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

#include <Urho3D/Urho3D.h>

#ifdef URHO3D_STATIC
    #define PLUGIN_RBFX_SCRIPT_API
#else
    #if Plugin_Rbfx_Script_EXPORT
        #define PLUGIN_RBFX_SCRIPT_API URHO3D_EXPORT_API
    #else
        #define PLUGIN_RBFX_SCRIPT_API URHO3D_IMPORT_API
    #endif
#endif

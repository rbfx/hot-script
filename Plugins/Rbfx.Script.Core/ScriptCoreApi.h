// Copyright (c) 2026 the rbfx-script project.
// This work is licensed under the terms of the MIT license.

#pragma once

// The export macro of Rbfx.Script.Core. The core is a shared library when the engine is shared.
// Then the process has one copy of the core and of the Hot++ runtime state, also when the game
// plugin and the editor plugin load into one process.
#if defined(RBFX_SCRIPT_CORE_SHARED)
    #if defined(_WIN32)
        #if defined(RBFX_SCRIPT_CORE_EXPORTS)
            #define RBFX_SCRIPT_CORE_API __declspec(dllexport)
        #else
            #define RBFX_SCRIPT_CORE_API __declspec(dllimport)
        #endif
    #else
        #define RBFX_SCRIPT_CORE_API __attribute__((visibility("default")))
    #endif
#else
    #define RBFX_SCRIPT_CORE_API
#endif

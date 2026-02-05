// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Common {

#ifdef __ANDROID__
    /**
 * Pin the current thread to big cores (performance cores)
 * On Snapdragon 8 Gen 2: Cores 4-7 (Cortex-X3 + A720)
 */
void SetBigCoreAffinity();

/**
 * Pin the current thread to little cores (efficiency cores)
 * On Snapdragon 8 Gen 2: Cores 0-3 (Cortex-A510)
 */
void SetLittleCoreAffinity();

/**
 * Allow the current thread to run on any core (reset affinity)
 */
void ResetCoreAffinity();

#else
// Stubs pour les autres plateformes
    inline void SetBigCoreAffinity() {}
    inline void SetLittleCoreAffinity() {}
    inline void ResetCoreAffinity() {}
#endif

} // namespace Common
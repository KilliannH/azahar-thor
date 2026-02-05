// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/cpu_affinity.h"

#ifdef __ANDROID__
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "common/logging/log.h"

namespace Common {

// Helper pour obtenir le TID (Thread ID) Linux
static pid_t GetThreadId() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

void SetBigCoreAffinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // Snapdragon 8 Gen 2 topology:
    // Cores 0-3: A510 (efficiency)
    // Cores 4-6: A720 (performance)
    // Core 7: X3 (prime)

    // Pin to big cores (4-7)
    for (int i = 4; i < 8; i++) {
        CPU_SET(i, &cpuset);
    }

    pid_t tid = GetThreadId();
    int result = sched_setaffinity(tid, sizeof(cpuset), &cpuset);

    if (result != 0) {
        LOG_WARNING(Common, "Failed to set big core affinity for TID {}: error {}", tid, errno);
    } else {
        LOG_INFO(Common, "Thread {} pinned to big cores (4-7)", tid);
    }
}

void SetLittleCoreAffinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // Pin to little cores (0-3)
    for (int i = 0; i < 4; i++) {
        CPU_SET(i, &cpuset);
    }

    pid_t tid = GetThreadId();
    int result = sched_setaffinity(tid, sizeof(cpuset), &cpuset);

    if (result != 0) {
        LOG_WARNING(Common, "Failed to set little core affinity for TID {}: error {}", tid, errno);
    } else {
        LOG_DEBUG(Common, "Thread {} pinned to little cores (0-3)", tid);
    }
}

void ResetCoreAffinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // Allow all cores
    int num_cores = sysconf(_SC_NPROCESSORS_CONF);
    for (int i = 0; i < num_cores; i++) {
        CPU_SET(i, &cpuset);
    }

    pid_t tid = GetThreadId();
    int result = sched_setaffinity(tid, sizeof(cpuset), &cpuset);

    if (result != 0) {
        LOG_WARNING(Common, "Failed to reset core affinity for TID {}: error {}", tid, errno);
    } else {
        LOG_DEBUG(Common, "Thread {} affinity reset (all {} cores)", tid, num_cores);
    }
}

} // namespace Common

#endif // __ANDROID__
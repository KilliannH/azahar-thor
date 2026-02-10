// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/cpu_affinity.h"

#ifdef __ANDROID__
#include <algorithm>
#include <fstream>
#include <sched.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include "common/logging/log.h"

namespace Common {

static pid_t GetThreadId() {
    return static_cast<pid_t>(syscall(SYS_gettid));
}

/**
 * Read the max frequency of a given CPU core from sysfs.
 * Returns 0 on failure.
 */
static long ReadCoreMaxFreq(int core_id) {
    const std::string path =
        "/sys/devices/system/cpu/cpu" + std::to_string(core_id) + "/cpufreq/cpuinfo_max_freq";
    std::ifstream file(path);
    long freq = 0;
    if (file.is_open()) {
        file >> freq;
    }
    return freq;
}

/**
 * Detect CPU topology at runtime by reading sysfs frequencies.
 * Splits cores into "big" (top 50% by frequency) and "little" (bottom 50%).
 */
struct CpuTopology {
    std::vector<int> big_cores;
    std::vector<int> little_cores;
};

static CpuTopology DetectTopology() {
    CpuTopology topo;
    const int num_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_CONF));

    // Collect (core_id, max_freq) pairs
    struct CoreInfo {
        int id;
        long freq;
    };
    std::vector<CoreInfo> cores;
    cores.reserve(num_cores);

    for (int i = 0; i < num_cores; i++) {
        long freq = ReadCoreMaxFreq(i);
        cores.push_back({i, freq});
    }

    if (cores.empty()) {
        LOG_WARNING(Common, "Could not read any CPU frequencies, falling back to all cores");
        for (int i = 0; i < num_cores; i++) {
            topo.big_cores.push_back(i);
        }
        return topo;
    }

    // Find the max frequency to determine the threshold
    long max_freq = 0;
    for (const auto& c : cores) {
        max_freq = std::max(max_freq, c.freq);
    }

    if (max_freq == 0) {
        // All frequencies are 0 (permission denied?), treat all as big
        LOG_WARNING(Common, "All CPU frequencies read as 0, falling back to all cores");
        for (int i = 0; i < num_cores; i++) {
            topo.big_cores.push_back(i);
        }
        return topo;
    }

    // Threshold: cores with >= 70% of max freq are "big"
    const long threshold = static_cast<long>(max_freq * 0.7);

    for (const auto& c : cores) {
        if (c.freq >= threshold) {
            topo.big_cores.push_back(c.id);
        } else {
            topo.little_cores.push_back(c.id);
        }
    }

    // Log the detected topology
    std::string big_str, little_str;
    for (int id : topo.big_cores) {
        big_str += std::to_string(id) + " ";
    }
    for (int id : topo.little_cores) {
        little_str += std::to_string(id) + " ";
    }
    LOG_INFO(Common, "Detected CPU topology: big cores=[{}], little cores=[{}]",
             big_str, little_str);

    return topo;
}

// Cache the topology (detected once, reused forever)
static const CpuTopology& GetTopology() {
    static CpuTopology topo = DetectTopology();
    return topo;
}

void SetBigCoreAffinity() {
    const auto& topo = GetTopology();
    if (topo.big_cores.empty()) {
        return;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int core : topo.big_cores) {
        CPU_SET(core, &cpuset);
    }

    pid_t tid = GetThreadId();
    int result = sched_setaffinity(tid, sizeof(cpuset), &cpuset);

    if (result != 0) {
        LOG_WARNING(Common, "Failed to set big core affinity for TID {}: error {}", tid, errno);
    } else {
        LOG_INFO(Common, "Thread {} pinned to big cores", tid);
    }
}

void SetLittleCoreAffinity() {
    const auto& topo = GetTopology();
    if (topo.little_cores.empty()) {
        return;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int core : topo.little_cores) {
        CPU_SET(core, &cpuset);
    }

    pid_t tid = GetThreadId();
    int result = sched_setaffinity(tid, sizeof(cpuset), &cpuset);

    if (result != 0) {
        LOG_WARNING(Common, "Failed to set little core affinity for TID {}: error {}", tid, errno);
    } else {
        LOG_DEBUG(Common, "Thread {} pinned to little cores", tid);
    }
}

void ResetCoreAffinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    int num_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_CONF));
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

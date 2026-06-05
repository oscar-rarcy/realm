#pragma once

#include <algorithm>

struct FixedTickPlan {
    int ticksToRun = 0;
    double nextTickMs = 0.0;
};

inline FixedTickPlan planFixedTicks(double nowMs,
                                    double nextTickMs,
                                    int tickMs,
                                    int maxTicksPerFrame = 2,
                                    int maxBacklogTicks = 4) {
    if (tickMs <= 0) return {0, nowMs};

    const double tick = (double)tickMs;
    const int maxTicks = std::max(1, maxTicksPerFrame);
    const double maxBacklogMs = tick * (double)std::max(1, maxBacklogTicks);

    if (nowMs - nextTickMs >= maxBacklogMs) {
        nextTickMs = nowMs;
    }

    int ticks = 0;
    while (nowMs >= nextTickMs && ticks < maxTicks) {
        nextTickMs += tick;
        ticks++;
    }

    if (ticks == maxTicks && nowMs >= nextTickMs) {
        nextTickMs = nowMs + tick;
    }

    return {ticks, nextTickMs};
}

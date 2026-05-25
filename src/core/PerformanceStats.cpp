#include "PerformanceStats.h"

#include <algorithm>

void PerformanceStats::recordFrame(double elapsedMs)
{
    snapshot_.frameMs = smooth(snapshot_.frameMs, elapsedMs, 0.2);
    if (snapshot_.frameMs > 0.001) {
        snapshot_.fps = 1000.0 / snapshot_.frameMs;
    }
}

void PerformanceStats::recordStroke(double elapsedMs, const QRect &dirtyRect, int samples)
{
    snapshot_.strokeMs = smooth(snapshot_.strokeMs, elapsedMs, 0.35);
    snapshot_.lastDirtyRect = dirtyRect;
    snapshot_.strokeSamples += std::max(0, samples);
}

void PerformanceStats::recordInput(const TabletInputEvent &event)
{
    snapshot_.lastPressure = event.pressure;
    ++snapshot_.inputEvents;

    if (lastInputTimestampNs_ > 0 && event.timestampNs > lastInputTimestampNs_) {
        const double intervalNs = static_cast<double>(event.timestampNs - lastInputTimestampNs_);
        if (intervalNs > 0.0) {
            const double hz = 1'000'000'000.0 / intervalNs;
            snapshot_.inputHz = smooth(snapshot_.inputHz, hz, 0.25);
        }
    }
    lastInputTimestampNs_ = event.timestampNs;
}

StatsSnapshot PerformanceStats::snapshot() const
{
    return snapshot_;
}

void PerformanceStats::reset()
{
    snapshot_ = {};
    lastInputTimestampNs_ = 0;
}

double PerformanceStats::smooth(double previous, double next, double alpha)
{
    if (previous <= 0.0) {
        return next;
    }
    return previous * (1.0 - alpha) + next * alpha;
}

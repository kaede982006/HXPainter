#pragma once

#include "TabletInputEvent.h"

#include <QRect>

struct StatsSnapshot {
    double fps = 0.0;
    double frameMs = 0.0;
    double strokeMs = 0.0;
    double inputHz = 0.0;
    double lastPressure = 0.0;
    int inputEvents = 0;
    int strokeSamples = 0;
    QRect lastDirtyRect;
};

class PerformanceStats {
public:
    void recordFrame(double elapsedMs);
    void recordStroke(double elapsedMs, const QRect &dirtyRect, int samples);
    void recordInput(const TabletInputEvent &event);

    [[nodiscard]] StatsSnapshot snapshot() const;
    void reset();

private:
    static double smooth(double previous, double next, double alpha);

    StatsSnapshot snapshot_;
    std::int64_t lastInputTimestampNs_ = 0;
};

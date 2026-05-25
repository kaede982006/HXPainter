#pragma once

#include "brush/BrushSettings.h"
#include "core/TabletInputEvent.h"

#include <QImage>
#include <QRect>

#include <optional>

struct EraserSettings {
    double radius = 24.0;
    double opacity = 1.0;
    double spacing = 0.18;
    bool pressureControlsRadius = true;
    bool eraseAlphaOnly = true;
    bool eraseCurrentLayerOnly = true;
};

struct EraserRenderResult {
    QRect dirtyRect;
    int samples = 0;

    [[nodiscard]] bool hasChanges() const
    {
        return samples > 0 && dirtyRect.isValid();
    }
};

class EraserEngine {
public:
    EraserRenderResult beginStroke(QImage &target, const StrokeSample &sample, const EraserSettings &settings);
    EraserRenderResult continueStroke(QImage &target, const StrokeSample &sample, const EraserSettings &settings);
    void endStroke();

private:
    static double effectiveRadius(const EraserSettings &settings, double pressure);
    static QRect stamp(QImage &target, const StrokeSample &sample, const EraserSettings &settings);

    std::optional<StrokeSample> lastSample_;
};

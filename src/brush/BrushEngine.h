#pragma once

#include "brush/BrushSettings.h"
#include "core/TabletInputEvent.h"

#include <QImage>
#include <QRect>

#include <optional>

struct BrushRenderResult {
    QRect dirtyRect;
    int samples = 0;

    [[nodiscard]] bool hasChanges() const
    {
        return samples > 0 && dirtyRect.isValid();
    }
};

class BrushEngine {
public:
    BrushRenderResult beginStroke(QImage &target, const StrokeSample &sample, const BrushSettings &settings);
    BrushRenderResult continueStroke(QImage &target, const StrokeSample &sample, const BrushSettings &settings);
    void endStroke();

private:
    static double effectiveRadius(const BrushSettings &settings, double pressure);
    static double effectiveOpacity(const BrushSettings &settings, double pressure);
    static QRect stamp(QImage &target, const StrokeSample &sample, const BrushSettings &settings);

    std::optional<StrokeSample> lastSample_;
};

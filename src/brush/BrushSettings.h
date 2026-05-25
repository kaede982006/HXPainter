#pragma once

#include <QColor>

struct BrushSettings {
    QColor color = QColor(28, 29, 31);
    double radius = 12.0;
    double opacity = 1.0;
    double flow = 1.0;
    double hardness = 0.72;
    double spacing = 0.22;
    bool pressureControlsRadius = true;
    bool pressureControlsOpacity = false;
};

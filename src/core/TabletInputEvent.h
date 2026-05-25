#pragma once

#include <QPointF>
#include <Qt>

#include <cstdint>

enum class InputDeviceKind {
    Mouse,
    Stylus,
    Eraser,
    Unknown
};

enum class InputEventKind {
    Press,
    Move,
    Release,
    Hover
};

struct TabletInputEvent {
    QPointF widgetPosition;
    QPointF canvasPosition;
    double pressure = 1.0;
    double tiltX = 0.0;
    double tiltY = 0.0;
    Qt::MouseButtons buttons = Qt::NoButton;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    InputDeviceKind device = InputDeviceKind::Unknown;
    InputEventKind kind = InputEventKind::Hover;
    std::int64_t timestampNs = 0;

    [[nodiscard]] bool isPrimaryDown() const
    {
        return buttons.testFlag(Qt::LeftButton);
    }
};

struct StrokeSample {
    QPointF position;
    double pressure = 1.0;
    std::int64_t timestampNs = 0;
};

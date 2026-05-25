#include "TabletInputMapper.h"

#include <QElapsedTimer>
#include <QPointingDevice>

#include <algorithm>

TabletInputEvent TabletInputMapper::fromTabletEvent(const QTabletEvent &event, const QPointF &canvasPosition)
{
    TabletInputEvent mapped;
    mapped.widgetPosition = event.position();
    mapped.canvasPosition = canvasPosition;
    mapped.pressure = std::clamp(static_cast<double>(event.pressure()), 0.0, 1.0);
    mapped.tiltX = event.xTilt();
    mapped.tiltY = event.yTilt();
    mapped.buttons = event.buttons();
    mapped.modifiers = event.modifiers();
    mapped.device = event.pointerType() == QPointingDevice::PointerType::Eraser
        ? InputDeviceKind::Eraser
        : InputDeviceKind::Stylus;
    mapped.kind = eventKind(event.type());
    mapped.timestampNs = nowNs();
    return mapped;
}

TabletInputEvent TabletInputMapper::fromMouseEvent(const QMouseEvent &event, const QPointF &canvasPosition)
{
    TabletInputEvent mapped;
    mapped.widgetPosition = event.position();
    mapped.canvasPosition = canvasPosition;
    mapped.pressure = event.buttons().testFlag(Qt::LeftButton) ? 1.0 : 0.0;
    mapped.buttons = event.buttons();
    if (event.type() == QEvent::MouseButtonPress) {
        mapped.buttons |= event.button();
        if (event.button() == Qt::LeftButton) {
            mapped.pressure = 1.0;
        }
    }
    mapped.modifiers = event.modifiers();
    mapped.device = InputDeviceKind::Mouse;
    mapped.kind = eventKind(event.type());
    mapped.timestampNs = nowNs();
    return mapped;
}

InputEventKind TabletInputMapper::eventKind(QEvent::Type type)
{
    switch (type) {
    case QEvent::TabletPress:
    case QEvent::MouseButtonPress:
        return InputEventKind::Press;
    case QEvent::TabletRelease:
    case QEvent::MouseButtonRelease:
        return InputEventKind::Release;
    case QEvent::TabletMove:
    case QEvent::MouseMove:
        return InputEventKind::Move;
    default:
        return InputEventKind::Hover;
    }
}

std::int64_t TabletInputMapper::nowNs()
{
    static const qint64 start = [] {
        QElapsedTimer timer;
        timer.start();
        return timer.msecsSinceReference();
    }();

    QElapsedTimer timer;
    timer.start();
    const qint64 currentMs = timer.msecsSinceReference();
    return static_cast<std::int64_t>((currentMs - start) * 1'000'000);
}

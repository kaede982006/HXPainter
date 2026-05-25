#pragma once

#include "core/TabletInputEvent.h"

#include <QMouseEvent>
#include <QTabletEvent>

class TabletInputMapper {
public:
    static TabletInputEvent fromTabletEvent(const QTabletEvent &event, const QPointF &canvasPosition);
    static TabletInputEvent fromMouseEvent(const QMouseEvent &event, const QPointF &canvasPosition);

private:
    static InputEventKind eventKind(QEvent::Type type);
    static std::int64_t nowNs();
};

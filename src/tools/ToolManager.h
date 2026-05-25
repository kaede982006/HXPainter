#pragma once

#include "tools/Tool.h"

#include <QObject>
#include <QVector>

class ToolManager final : public QObject {
    Q_OBJECT

public:
    explicit ToolManager(QObject *parent = nullptr);

    [[nodiscard]] ToolType activeToolType() const;
    [[nodiscard]] Tool *activeTool() const;
    [[nodiscard]] Tool *tool(ToolType type) const;
    [[nodiscard]] QVector<Tool *> tools() const;

public Q_SLOTS:
    void setActiveTool(ToolType type);

Q_SIGNALS:
    void activeToolChanged(ToolType type);

private:
    QVector<Tool *> tools_;
    ToolType activeTool_ = ToolType::Brush;
};

#pragma once

#include <QCursor>
#include <QKeySequence>
#include <QObject>
#include <QString>

enum class ToolType {
    Brush,
    Eraser,
    Fill,
    ColorPicker,
    Move,
    Selection,
    Transform,
    Shape,
    Text,
    Hand,
    Zoom
};

QString toolTypeName(ToolType type);

class Tool : public QObject {
    Q_OBJECT

public:
    Tool(ToolType type, QString name, QKeySequence shortcut, QObject *parent = nullptr);

    [[nodiscard]] ToolType type() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QKeySequence shortcut() const;
    [[nodiscard]] virtual QCursor cursor() const;

public Q_SLOTS:
    virtual void cancel();

private:
    ToolType type_;
    QString name_;
    QKeySequence shortcut_;
};

class BrushTool final : public Tool {
    Q_OBJECT
public:
    explicit BrushTool(QObject *parent = nullptr);
};

class EraserTool final : public Tool {
    Q_OBJECT
public:
    explicit EraserTool(QObject *parent = nullptr);
};

class FillTool final : public Tool {
    Q_OBJECT
public:
    explicit FillTool(QObject *parent = nullptr);
};

class ColorPickerTool final : public Tool {
    Q_OBJECT
public:
    explicit ColorPickerTool(QObject *parent = nullptr);
};

class MoveTool final : public Tool {
    Q_OBJECT
public:
    explicit MoveTool(QObject *parent = nullptr);
};

class SelectionTool final : public Tool {
    Q_OBJECT
public:
    explicit SelectionTool(QObject *parent = nullptr);
};

class TransformTool final : public Tool {
    Q_OBJECT
public:
    explicit TransformTool(QObject *parent = nullptr);
};

class ShapeTool final : public Tool {
    Q_OBJECT
public:
    explicit ShapeTool(QObject *parent = nullptr);
};

class TextTool final : public Tool {
    Q_OBJECT
public:
    explicit TextTool(QObject *parent = nullptr);
};

class HandTool final : public Tool {
    Q_OBJECT
public:
    explicit HandTool(QObject *parent = nullptr);
    [[nodiscard]] QCursor cursor() const override;
};

class ZoomTool final : public Tool {
    Q_OBJECT
public:
    explicit ZoomTool(QObject *parent = nullptr);
};

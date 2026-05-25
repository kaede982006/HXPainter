#include "Tool.h"

QString toolTypeName(ToolType type)
{
    switch (type) {
    case ToolType::Brush:
        return QStringLiteral("Brush");
    case ToolType::Eraser:
        return QStringLiteral("Eraser");
    case ToolType::Fill:
        return QStringLiteral("Fill");
    case ToolType::ColorPicker:
        return QStringLiteral("Color Picker");
    case ToolType::Move:
        return QStringLiteral("Move");
    case ToolType::Selection:
        return QStringLiteral("Selection");
    case ToolType::Transform:
        return QStringLiteral("Transform");
    case ToolType::Shape:
        return QStringLiteral("Shape");
    case ToolType::Text:
        return QStringLiteral("Text");
    case ToolType::Hand:
        return QStringLiteral("Hand");
    case ToolType::Zoom:
        return QStringLiteral("Zoom");
    }
    return QStringLiteral("Unknown");
}

Tool::Tool(ToolType type, QString name, QKeySequence shortcut, QObject *parent)
    : QObject(parent)
    , type_(type)
    , name_(std::move(name))
    , shortcut_(std::move(shortcut))
{
}

ToolType Tool::type() const
{
    return type_;
}

QString Tool::name() const
{
    return name_;
}

QKeySequence Tool::shortcut() const
{
    return shortcut_;
}

QCursor Tool::cursor() const
{
    return Qt::CrossCursor;
}

void Tool::cancel()
{
}

BrushTool::BrushTool(QObject *parent)
    : Tool(ToolType::Brush, QStringLiteral("Brush"), QKeySequence(QStringLiteral("B")), parent)
{
}

EraserTool::EraserTool(QObject *parent)
    : Tool(ToolType::Eraser, QStringLiteral("Eraser"), QKeySequence(QStringLiteral("E")), parent)
{
}

FillTool::FillTool(QObject *parent)
    : Tool(ToolType::Fill, QStringLiteral("Fill"), QKeySequence(QStringLiteral("F")), parent)
{
}

ColorPickerTool::ColorPickerTool(QObject *parent)
    : Tool(ToolType::ColorPicker, QStringLiteral("Color Picker"), QKeySequence(QStringLiteral("I")), parent)
{
}

MoveTool::MoveTool(QObject *parent)
    : Tool(ToolType::Move, QStringLiteral("Move"), QKeySequence(QStringLiteral("V")), parent)
{
}

SelectionTool::SelectionTool(QObject *parent)
    : Tool(ToolType::Selection, QStringLiteral("Selection"), QKeySequence(QStringLiteral("M")), parent)
{
}

TransformTool::TransformTool(QObject *parent)
    : Tool(ToolType::Transform, QStringLiteral("Transform"), QKeySequence(QStringLiteral("Ctrl+T")), parent)
{
}

ShapeTool::ShapeTool(QObject *parent)
    : Tool(ToolType::Shape, QStringLiteral("Shape"), QKeySequence(QStringLiteral("U")), parent)
{
}

TextTool::TextTool(QObject *parent)
    : Tool(ToolType::Text, QStringLiteral("Text"), QKeySequence(QStringLiteral("T")), parent)
{
}

HandTool::HandTool(QObject *parent)
    : Tool(ToolType::Hand, QStringLiteral("Hand"), QKeySequence(QStringLiteral("H")), parent)
{
}

QCursor HandTool::cursor() const
{
    return Qt::OpenHandCursor;
}

ZoomTool::ZoomTool(QObject *parent)
    : Tool(ToolType::Zoom, QStringLiteral("Zoom"), QKeySequence(QStringLiteral("Z")), parent)
{
}

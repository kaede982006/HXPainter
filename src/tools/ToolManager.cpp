#include "ToolManager.h"

ToolManager::ToolManager(QObject *parent)
    : QObject(parent)
{
    tools_ = {
        new BrushTool(this),
        new EraserTool(this),
        new FillTool(this),
        new ColorPickerTool(this),
        new MoveTool(this),
        new SelectionTool(this),
        new TransformTool(this),
        new ShapeTool(this),
        new TextTool(this),
        new HandTool(this),
        new ZoomTool(this),
    };
}

ToolType ToolManager::activeToolType() const
{
    return activeTool_;
}

Tool *ToolManager::activeTool() const
{
    return tool(activeTool_);
}

Tool *ToolManager::tool(ToolType type) const
{
    for (Tool *candidate : tools_) {
        if (candidate->type() == type) {
            return candidate;
        }
    }
    return nullptr;
}

QVector<Tool *> ToolManager::tools() const
{
    return tools_;
}

void ToolManager::setActiveTool(ToolType type)
{
    if (activeTool_ == type) {
        return;
    }
    activeTool_ = type;
    Q_EMIT activeToolChanged(type);
}

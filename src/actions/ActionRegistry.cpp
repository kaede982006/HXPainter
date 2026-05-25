#include "ActionRegistry.h"

#include <QIcon>

namespace {
QIcon icon(const QString &resourcePath)
{
    return QIcon(resourcePath);
}
}

ActionRegistry::ActionRegistry(QObject *parent)
    : QObject(parent)
{
    newDocument = makeAction(QStringLiteral("&New"), QKeySequence::New, QStringLiteral("Create a new document"));
    open = makeAction(QStringLiteral("&Open/Load..."), QKeySequence::Open, QStringLiteral("Open an HXP project or image"));
    importImage = makeAction(QStringLiteral("&Import..."), QKeySequence(QStringLiteral("Ctrl+I")), QStringLiteral("Import image into the current document"));
    save = makeAction(QStringLiteral("&Save"), QKeySequence::Save, QStringLiteral("Save current HXP project"));
    saveAs = makeAction(QStringLiteral("Save &As..."), QKeySequence::SaveAs, QStringLiteral("Save current project under a new path"));
    exportImage = makeAction(QStringLiteral("&Export..."), QKeySequence(QStringLiteral("Ctrl+E")), QStringLiteral("Export merged image"));
    closeDocument = makeAction(QStringLiteral("&Close Document"), QKeySequence(QStringLiteral("Ctrl+W")));
    exit = makeAction(QStringLiteral("E&xit"), QKeySequence::Quit);

    undo = makeAction(QStringLiteral("&Undo"), QKeySequence::Undo);
    redo = makeAction(QStringLiteral("&Redo"), QKeySequence(QStringLiteral("Ctrl+Shift+Z")));
    redo->setShortcuts({QKeySequence(QStringLiteral("Ctrl+Shift+Z")), QKeySequence(QStringLiteral("Ctrl+Y"))});
    cut = makeAction(QStringLiteral("Cu&t"), QKeySequence::Cut);
    copy = makeAction(QStringLiteral("&Copy"), QKeySequence::Copy);
    paste = makeAction(QStringLiteral("&Paste"), QKeySequence::Paste);
    clear = makeAction(QStringLiteral("&Clear"), QKeySequence::Delete);
    deselect = makeAction(QStringLiteral("&Deselect"), QKeySequence(QStringLiteral("Ctrl+D")));
    preferences = makeAction(QStringLiteral("&Preferences..."));

    zoomIn = makeAction(QStringLiteral("Zoom &In"), QKeySequence::ZoomIn);
    zoomOut = makeAction(QStringLiteral("Zoom &Out"), QKeySequence::ZoomOut);
    fitCanvas = makeAction(QStringLiteral("&Fit Canvas"), QKeySequence(QStringLiteral("Ctrl+0")));
    rotateLeft = makeAction(QStringLiteral("Rotate &Left"), QKeySequence(QStringLiteral("Ctrl+[")), QStringLiteral("Rotate the canvas view left"));
    rotateRight = makeAction(QStringLiteral("Rotate &Right"), QKeySequence(QStringLiteral("Ctrl+]")), QStringLiteral("Rotate the canvas view right"));
    resetView = makeAction(QStringLiteral("&Reset View"));
    filterGallery = makeAction(QStringLiteral("&Filter Gallery..."));

    addLayer = makeAction(QStringLiteral("&Add Layer"), QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    deleteLayer = makeAction(QStringLiteral("&Delete Layer"));
    duplicateLayer = makeAction(QStringLiteral("D&uplicate Layer"));
    renameLayer = makeAction(QStringLiteral("&Rename Layer"));
    moveLayerUp = makeAction(QStringLiteral("Move Layer &Up"));
    moveLayerDown = makeAction(QStringLiteral("Move Layer &Down"));
    mergeLayerDown = makeAction(QStringLiteral("&Merge Down"));
    toggleLayerVisibility = makeAction(QStringLiteral("Toggle &Visibility"));
    toggleLayerLock = makeAction(QStringLiteral("Toggle &Lock"));
    layerOpacity = makeAction(QStringLiteral("Layer &Opacity"));
    blendMode = makeAction(QStringLiteral("&Blend Mode"));

    toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    brush = makeToolAction(ToolType::Brush, QStringLiteral("Brush"), QKeySequence(QStringLiteral("B")));
    eraser = makeToolAction(ToolType::Eraser, QStringLiteral("Eraser"), QKeySequence(QStringLiteral("E")));
    fill = makeToolAction(ToolType::Fill, QStringLiteral("Fill"), QKeySequence(QStringLiteral("F")));
    colorPicker = makeToolAction(ToolType::ColorPicker, QStringLiteral("Color Picker"), QKeySequence(QStringLiteral("I")));
    move = makeToolAction(ToolType::Move, QStringLiteral("Move"), QKeySequence(QStringLiteral("V")));
    selection = makeToolAction(ToolType::Selection, QStringLiteral("Selection"), QKeySequence(QStringLiteral("M")));
    transform = makeToolAction(ToolType::Transform, QStringLiteral("Transform"), QKeySequence(QStringLiteral("Ctrl+T")));
    shape = makeToolAction(ToolType::Shape, QStringLiteral("Shape"), QKeySequence(QStringLiteral("U")));
    text = makeToolAction(ToolType::Text, QStringLiteral("Text"), QKeySequence(QStringLiteral("T")));
    hand = makeToolAction(ToolType::Hand, QStringLiteral("Hand"), QKeySequence(QStringLiteral("H")));
    zoom = makeToolAction(ToolType::Zoom, QStringLiteral("Zoom"), QKeySequence(QStringLiteral("Z")));
    brush->setChecked(true);

    newDocument->setIcon(icon(QStringLiteral(":/icons/file-plus.svg")));
    open->setIcon(icon(QStringLiteral(":/icons/folder-open.svg")));
    importImage->setIcon(icon(QStringLiteral(":/icons/file-import.svg")));
    save->setIcon(icon(QStringLiteral(":/icons/device-floppy.svg")));
    saveAs->setIcon(icon(QStringLiteral(":/icons/device-floppy.svg")));
    exportImage->setIcon(icon(QStringLiteral(":/icons/file-export.svg")));
    closeDocument->setIcon(icon(QStringLiteral(":/icons/x.svg")));

    undo->setIcon(icon(QStringLiteral(":/icons/arrow-back-up.svg")));
    redo->setIcon(icon(QStringLiteral(":/icons/arrow-forward-up.svg")));
    clear->setIcon(icon(QStringLiteral(":/icons/trash.svg")));
    preferences->setIcon(icon(QStringLiteral(":/icons/settings.svg")));
    deselect->setIcon(icon(QStringLiteral(":/icons/x.svg")));

    zoomIn->setIcon(icon(QStringLiteral(":/icons/zoom-in.svg")));
    zoomOut->setIcon(icon(QStringLiteral(":/icons/zoom-out.svg")));
    fitCanvas->setIcon(icon(QStringLiteral(":/icons/arrow-autofit-content.svg")));
    rotateLeft->setIcon(icon(QStringLiteral(":/icons/arrow-back-up.svg")));
    rotateRight->setIcon(icon(QStringLiteral(":/icons/arrow-forward-up.svg")));
    resetView->setIcon(icon(QStringLiteral(":/icons/zoom-reset.svg")));
    filterGallery->setIcon(icon(QStringLiteral(":/icons/adjustments.svg")));

    addLayer->setIcon(icon(QStringLiteral(":/icons/plus.svg")));
    deleteLayer->setIcon(icon(QStringLiteral(":/icons/minus.svg")));
    duplicateLayer->setIcon(icon(QStringLiteral(":/icons/copy-plus.svg")));
    renameLayer->setIcon(icon(QStringLiteral(":/icons/letter-t.svg")));
    moveLayerUp->setIcon(icon(QStringLiteral(":/icons/arrow-up.svg")));
    moveLayerDown->setIcon(icon(QStringLiteral(":/icons/arrow-down.svg")));
    mergeLayerDown->setIcon(icon(QStringLiteral(":/icons/git-merge.svg")));
    toggleLayerVisibility->setIcon(icon(QStringLiteral(":/icons/eye.svg")));
    toggleLayerLock->setIcon(icon(QStringLiteral(":/icons/lock.svg")));
    layerOpacity->setIcon(icon(QStringLiteral(":/icons/droplet.svg")));
    blendMode->setIcon(icon(QStringLiteral(":/icons/blend-mode.svg")));

    brush->setIcon(icon(QStringLiteral(":/icons/brush.svg")));
    eraser->setIcon(icon(QStringLiteral(":/icons/eraser.svg")));
    fill->setIcon(icon(QStringLiteral(":/icons/bucket.svg")));
    colorPicker->setIcon(icon(QStringLiteral(":/icons/color-picker.svg")));
    move->setIcon(icon(QStringLiteral(":/icons/pointer.svg")));
    selection->setIcon(icon(QStringLiteral(":/icons/select.svg")));
    transform->setIcon(icon(QStringLiteral(":/icons/arrows-maximize.svg")));
    shape->setIcon(icon(QStringLiteral(":/icons/square.svg")));
    text->setIcon(icon(QStringLiteral(":/icons/letter-t.svg")));
    hand->setIcon(icon(QStringLiteral(":/icons/hand-stop.svg")));
    zoom->setIcon(icon(QStringLiteral(":/icons/zoom-in.svg")));

    setDocumentAvailable(false);
    setUndoRedoAvailable(false, false);
}

QVector<QAction *> ActionRegistry::fileDocumentActions() const
{
    return {save, saveAs, exportImage, closeDocument};
}

QVector<QAction *> ActionRegistry::layerDocumentActions() const
{
    return {addLayer, deleteLayer, duplicateLayer, renameLayer, moveLayerUp, moveLayerDown,
            mergeLayerDown, toggleLayerVisibility, toggleLayerLock, layerOpacity, blendMode};
}

QVector<QAction *> ActionRegistry::toolActions() const
{
    return {brush, eraser, fill, colorPicker, move, selection, transform, shape, text, hand, zoom};
}

QVector<QAction *> ActionRegistry::sideToolActions() const
{
    return {brush, eraser, fill, colorPicker, selection, deselect, transform, shape, text};
}

QAction *ActionRegistry::toolAction(ToolType type) const
{
    switch (type) {
    case ToolType::Brush:
        return brush;
    case ToolType::Eraser:
        return eraser;
    case ToolType::Fill:
        return fill;
    case ToolType::ColorPicker:
        return colorPicker;
    case ToolType::Move:
        return move;
    case ToolType::Selection:
        return selection;
    case ToolType::Transform:
        return transform;
    case ToolType::Shape:
        return shape;
    case ToolType::Text:
        return text;
    case ToolType::Hand:
        return hand;
    case ToolType::Zoom:
        return zoom;
    }
    return nullptr;
}

void ActionRegistry::setDocumentAvailable(bool available)
{
    for (QAction *action : fileDocumentActions()) {
        action->setEnabled(available);
    }
    for (QAction *action : layerDocumentActions()) {
        action->setEnabled(available);
    }
    for (QAction *action : toolActions()) {
        action->setEnabled(available);
    }
    cut->setEnabled(available);
    copy->setEnabled(available);
    paste->setEnabled(available);
    clear->setEnabled(available);
    deselect->setEnabled(available);
    zoomIn->setEnabled(available);
    zoomOut->setEnabled(available);
    fitCanvas->setEnabled(available);
    rotateLeft->setEnabled(available);
    rotateRight->setEnabled(available);
    resetView->setEnabled(available);
    filterGallery->setEnabled(available);
}

void ActionRegistry::setUndoRedoAvailable(bool canUndo, bool canRedo)
{
    undo->setEnabled(canUndo);
    redo->setEnabled(canRedo);
}

QAction *ActionRegistry::makeAction(const QString &text, const QKeySequence &shortcut, const QString &tooltip)
{
    QAction *action = new QAction(text, this);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
        // Use WindowShortcut so shortcuts work globally.
        // QLineEdit and other widgets will still handle their own shortcuts first
        // because we handles focus correctly.
        action->setShortcutContext(Qt::WindowShortcut);
    }
    if (!tooltip.isEmpty()) {
        action->setToolTip(tooltip);
        action->setStatusTip(tooltip);
    }
    return action;
}

QAction *ActionRegistry::makeToolAction(ToolType type, const QString &text, const QKeySequence &shortcut)
{
    QAction *action = makeAction(text, shortcut, text);
    action->setCheckable(true);
    action->setData(static_cast<int>(type));
    toolGroup->addAction(action);
    return action;
}

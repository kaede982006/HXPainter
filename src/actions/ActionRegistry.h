#pragma once

#include "tools/Tool.h"

#include <QAction>
#include <QActionGroup>
#include <QObject>
#include <QPointer>
#include <QVector>

class ActionRegistry final : public QObject {
    Q_OBJECT

public:
    explicit ActionRegistry(QObject *parent = nullptr);

    QAction *newDocument = nullptr;
    QAction *open = nullptr;
    QAction *importImage = nullptr;
    QAction *save = nullptr;
    QAction *saveAs = nullptr;
    QAction *exportImage = nullptr;
    QAction *closeDocument = nullptr;
    QAction *exit = nullptr;

    QAction *undo = nullptr;
    QAction *redo = nullptr;
    QAction *cut = nullptr;
    QAction *copy = nullptr;
    QAction *paste = nullptr;
    QAction *clear = nullptr;
    QAction *deselect = nullptr;
    QAction *preferences = nullptr;

    QAction *zoomIn = nullptr;
    QAction *zoomOut = nullptr;
    QAction *fitCanvas = nullptr;
    QAction *rotateLeft = nullptr;
    QAction *rotateRight = nullptr;
    QAction *resetView = nullptr;
    QAction *filterGallery = nullptr;

    QAction *addLayer = nullptr;
    QAction *deleteLayer = nullptr;
    QAction *duplicateLayer = nullptr;
    QAction *renameLayer = nullptr;
    QAction *moveLayerUp = nullptr;
    QAction *moveLayerDown = nullptr;
    QAction *mergeLayerDown = nullptr;
    QAction *toggleLayerVisibility = nullptr;
    QAction *toggleLayerLock = nullptr;
    QAction *layerOpacity = nullptr;
    QAction *blendMode = nullptr;

    QAction *brush = nullptr;
    QAction *eraser = nullptr;
    QAction *fill = nullptr;
    QAction *colorPicker = nullptr;
    QAction *move = nullptr;
    QAction *selection = nullptr;
    QAction *transform = nullptr;
    QAction *shape = nullptr;
    QAction *text = nullptr;
    QAction *hand = nullptr;
    QAction *zoom = nullptr;

    QActionGroup *toolGroup = nullptr;

    [[nodiscard]] QVector<QAction *> fileDocumentActions() const;
    [[nodiscard]] QVector<QAction *> layerDocumentActions() const;
    [[nodiscard]] QVector<QAction *> toolActions() const;
    [[nodiscard]] QVector<QAction *> sideToolActions() const;
    [[nodiscard]] QAction *toolAction(ToolType type) const;

    void setDocumentAvailable(bool available);
    void setUndoRedoAvailable(bool canUndo, bool canRedo);

private:
    QAction *makeAction(const QString &text, const QKeySequence &shortcut = {}, const QString &tooltip = {});
    QAction *makeToolAction(ToolType type, const QString &text, const QKeySequence &shortcut);
};

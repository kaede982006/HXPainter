#pragma once

#include "commands/CommandManager.h"
#include "render/CanvasDocument.h"

#include <QObject>

class LayerManager final : public QObject {
    Q_OBJECT

public:
    explicit LayerManager(CanvasDocument *document, QObject *parent = nullptr);

    void setCommandManager(CommandManager *commandManager);
    [[nodiscard]] CanvasDocument *document() const;
    [[nodiscard]] int activeLayerIndex() const;
    [[nodiscard]] const Layer *activeLayer() const;

public Q_SLOTS:
    void setActiveLayer(int index);
    void addLayer();
    void deleteActiveLayer();
    void duplicateActiveLayer();
    void moveActiveLayerUp();
    void moveActiveLayerDown();
    void mergeActiveLayerDown();
    void renameActiveLayer(const QString &name);
    void setActiveLayerVisible(bool visible);
    void setActiveLayerLocked(bool locked);
    void setActiveLayerAlphaLock(bool alphaLock);
    void setActiveLayerOpacity(double opacity);
    void setActiveLayerBlendMode(const QString &blendMode);
    void importImageAsLayer(const QString &filePath);
    void notifyDocumentReset();

Q_SIGNALS:
    void layersChanged();
    void activeLayerChanged(int index);
    void statusMessage(const QString &message);

private:
    void executeSnapshot(const QString &name, const DocumentState &before, const DocumentState &after);

    CanvasDocument *document_ = nullptr;
    CommandManager *commandManager_ = nullptr;
};

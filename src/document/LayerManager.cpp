#include "LayerManager.h"

#include <QFileInfo>

LayerManager::LayerManager(CanvasDocument *document, QObject *parent)
    : QObject(parent)
    , document_(document)
{
}

void LayerManager::setCommandManager(CommandManager *commandManager)
{
    commandManager_ = commandManager;
}

CanvasDocument *LayerManager::document() const
{
    return document_;
}

int LayerManager::activeLayerIndex() const
{
    return document_ ? document_->activeLayerIndex() : -1;
}

const Layer *LayerManager::activeLayer() const
{
    return document_ ? document_->activeLayer() : nullptr;
}

void LayerManager::setActiveLayer(int index)
{
    if (!document_) {
        return;
    }
    document_->setActiveLayerIndex(index);
    Q_EMIT activeLayerChanged(document_->activeLayerIndex());
    Q_EMIT layersChanged();
}

void LayerManager::addLayer()
{
    if (!document_ || !document_->isValid()) {
        return;
    }
    const DocumentState before = document_->snapshot();
    document_->addLayer();
    executeSnapshot(QStringLiteral("Add Layer"), before, document_->snapshot());
}

void LayerManager::deleteActiveLayer()
{
    if (!document_ || !document_->isValid()) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (!document_->deleteLayer(document_->activeLayerIndex())) {
        Q_EMIT statusMessage(QStringLiteral("Cannot delete the last layer."));
        return;
    }
    executeSnapshot(QStringLiteral("Delete Layer"), before, document_->snapshot());
}

void LayerManager::duplicateActiveLayer()
{
    if (!document_ || !document_->isValid()) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->duplicateLayer(document_->activeLayerIndex()) < 0) {
        return;
    }
    executeSnapshot(QStringLiteral("Duplicate Layer"), before, document_->snapshot());
}

void LayerManager::moveActiveLayerUp()
{
    if (!document_) {
        return;
    }
    const int index = document_->activeLayerIndex();
    const DocumentState before = document_->snapshot();
    if (document_->moveLayer(index, index + 1)) {
        executeSnapshot(QStringLiteral("Move Layer Up"), before, document_->snapshot());
    }
}

void LayerManager::moveActiveLayerDown()
{
    if (!document_) {
        return;
    }
    const int index = document_->activeLayerIndex();
    const DocumentState before = document_->snapshot();
    if (document_->moveLayer(index, index - 1)) {
        executeSnapshot(QStringLiteral("Move Layer Down"), before, document_->snapshot());
    }
}

void LayerManager::mergeActiveLayerDown()
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->mergeLayerDown(document_->activeLayerIndex())) {
        executeSnapshot(QStringLiteral("Merge Layer"), before, document_->snapshot());
    }
}

void LayerManager::renameActiveLayer(const QString &name)
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->renameLayer(document_->activeLayerIndex(), name)) {
        executeSnapshot(QStringLiteral("Rename Layer"), before, document_->snapshot());
    }
}

void LayerManager::setActiveLayerVisible(bool visible)
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->setLayerVisible(document_->activeLayerIndex(), visible)) {
        executeSnapshot(QStringLiteral("Toggle Visibility"), before, document_->snapshot());
    }
}

void LayerManager::setActiveLayerLocked(bool locked)
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->setLayerLocked(document_->activeLayerIndex(), locked)) {
        executeSnapshot(QStringLiteral("Toggle Lock"), before, document_->snapshot());
    }
}

void LayerManager::setActiveLayerAlphaLock(bool alphaLock)
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->setLayerAlphaLock(document_->activeLayerIndex(), alphaLock)) {
        executeSnapshot(QStringLiteral("Toggle Alpha Lock"), before, document_->snapshot());
    }
}

void LayerManager::setActiveLayerOpacity(double opacity)
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->setLayerOpacity(document_->activeLayerIndex(), opacity)) {
        executeSnapshot(QStringLiteral("Change Layer Opacity"), before, document_->snapshot());
    }
}

void LayerManager::setActiveLayerBlendMode(const QString &blendMode)
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    if (document_->setLayerBlendMode(document_->activeLayerIndex(), blendMode)) {
        executeSnapshot(QStringLiteral("Change Blend Mode"), before, document_->snapshot());
    }
}

void LayerManager::importImageAsLayer(const QString &filePath)
{
    if (!document_) {
        return;
    }
    const DocumentState before = document_->snapshot();
    QString error;
    if (!document_->importImageAsLayer(filePath, &error)) {
        Q_EMIT statusMessage(error);
        return;
    }
    executeSnapshot(QStringLiteral("Import Image"), before, document_->snapshot());
    Q_EMIT statusMessage(QStringLiteral("Imported %1").arg(QFileInfo(filePath).fileName()));
}

void LayerManager::notifyDocumentReset()
{
    Q_EMIT activeLayerChanged(activeLayerIndex());
    Q_EMIT layersChanged();
}

void LayerManager::executeSnapshot(const QString &name, const DocumentState &before, const DocumentState &after)
{
    if (commandManager_) {
        commandManager_->execute(std::make_unique<DocumentSnapshotCommand>(name, document_, before, after));
    } else {
        document_->restore(after);
    }
    Q_EMIT activeLayerChanged(activeLayerIndex());
    Q_EMIT layersChanged();
}

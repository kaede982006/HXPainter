#include "CanvasDocument.h"

#include <QFileInfo>
#include <QPainter>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace {
DocumentState deepCopyStateImages(const DocumentState &state)
{
    DocumentState copy = state;
    for (Layer &layer : copy.layers) {
        if (!layer.image.isNull()) {
            layer.image = layer.image.copy();
        }
    }
    return copy;
}
}

QImage Layer::thumbnail(int size) const
{
    if (image.isNull()) {
        return {};
    }
    return image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void CanvasDocument::reset(const NewDocumentSettings &settings)
{
    invalidateCompositeCache();
    state_ = {};
    state_.canvasSize = QSize(std::max(1, settings.width), std::max(1, settings.height));
    state_.dpi = settings.dpi;
    state_.backgroundColor = settings.backgroundColor;
    state_.transparentBackground = settings.transparentBackground;
    state_.colorSpace = settings.colorSpace;
    state_.bitDepth = settings.bitDepth;

    const QColor firstFill = settings.transparentBackground ? QColor(Qt::transparent) : settings.backgroundColor;
    const int layerCount = std::max(1, settings.defaultLayerCount);
    for (int i = 0; i < layerCount; ++i) {
        const QString name = i == 0 && !settings.transparentBackground
            ? QStringLiteral("Background")
            : QStringLiteral("Layer %1").arg(i + 1);
        state_.layers.push_back(makeLayer(state_.canvasSize, name, i == 0 ? firstFill : QColor(Qt::transparent)));
    }
    state_.activeLayerIndex = state_.layers.size() - 1;
    state_.modified = false;
}

void CanvasDocument::reset(const QSize &size, const QColor &background)
{
    NewDocumentSettings settings;
    settings.width = size.width();
    settings.height = size.height();
    settings.backgroundColor = background;
    settings.transparentBackground = background.alpha() == 0;
    reset(settings);
}

bool CanvasDocument::loadImageAsDocument(const QString &filePath)
{
    QImage loaded;
    if (!loaded.load(filePath)) {
        return false;
    }

    invalidateCompositeCache();
    state_ = {};
    state_.canvasSize = loaded.size();
    state_.transparentBackground = true;
    state_.layers.push_back(makeLayer(loaded.size(), QFileInfo(filePath).completeBaseName()));
    state_.layers[0].image = loaded.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    state_.activeLayerIndex = 0;
    state_.filePath.clear();
    state_.modified = true;
    return true;
}

bool CanvasDocument::importImageAsLayer(const QString &filePath, QString *error)
{
    if (!isValid()) {
        if (error) {
            *error = QStringLiteral("No document is open.");
        }
        return false;
    }

    QImage imported;
    if (!imported.load(filePath)) {
        if (error) {
            *error = QStringLiteral("Failed to load image: %1").arg(filePath);
        }
        return false;
    }

    return addImageLayer(imported, QFileInfo(filePath).completeBaseName(), error, true);
}

bool CanvasDocument::addImageLayer(const QImage &image, const QString &name, QString *error, bool centered)
{
    if (!isValid()) {
        if (error) {
            *error = QStringLiteral("No document is open.");
        }
        return false;
    }
    if (image.isNull()) {
        if (error) {
            *error = QStringLiteral("Image is empty.");
        }
        return false;
    }

    const QString layerName = name.trimmed().isEmpty() ? QStringLiteral("Imported Image") : name.trimmed();
    Layer layer = makeLayer(size(), layerName);
    QPainter painter(&layer.image);
    const QImage converted = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QPoint topLeft = centered
        ? QPoint((size().width() - converted.width()) / 2, (size().height() - converted.height()) / 2)
        : QPoint(0, 0);
    painter.drawImage(topLeft, converted);
    state_.layers.push_back(layer);
    state_.activeLayerIndex = state_.layers.size() - 1;
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

bool CanvasDocument::exportCompositedImage(const QString &filePath, const QColor &matte) const
{
    QImage merged = compositedImage();
    if (merged.isNull()) {
        return false;
    }

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
        QImage flattened(merged.size(), QImage::Format_RGB32);
        flattened.fill(matte);
        QPainter painter(&flattened);
        painter.drawImage(QPoint(0, 0), merged);
        return flattened.save(filePath, "JPG", 92);
    }
    return merged.save(filePath);
}

QSize CanvasDocument::size() const
{
    return state_.canvasSize;
}

bool CanvasDocument::isValid() const
{
    return !state_.canvasSize.isEmpty() && !state_.layers.isEmpty();
}

bool CanvasDocument::isModified() const
{
    return state_.modified;
}

QString CanvasDocument::filePath() const
{
    return state_.filePath;
}

int CanvasDocument::activeLayerIndex() const
{
    return state_.activeLayerIndex;
}

const QVector<Layer> &CanvasDocument::layers() const
{
    return state_.layers;
}

QVector<Layer> &CanvasDocument::layers()
{
    return state_.layers;
}

const Layer *CanvasDocument::activeLayer() const
{
    if (!validLayerIndex(state_.activeLayerIndex)) {
        return nullptr;
    }
    return &state_.layers[state_.activeLayerIndex];
}

Layer *CanvasDocument::activeLayer()
{
    if (!validLayerIndex(state_.activeLayerIndex)) {
        return nullptr;
    }
    return &state_.layers[state_.activeLayerIndex];
}

QImage *CanvasDocument::activeImage()
{
    invalidateCompositeCache();
    Layer *layer = activeLayer();
    return layer ? &layer->image : nullptr;
}

const QImage *CanvasDocument::activeImage() const
{
    const Layer *layer = activeLayer();
    return layer ? &layer->image : nullptr;
}

QImage CanvasDocument::compositedImage(bool includeBackground) const
{
    if (!isValid()) {
        return {};
    }

    QImage &cache = includeBackground ? compositeWithBackground_ : compositeWithoutBackground_;
    bool &cacheValid = includeBackground ? compositeWithBackgroundValid_ : compositeWithoutBackgroundValid_;
    if (cacheValid) {
        return cache;
    }

    QImage merged(size(), QImage::Format_ARGB32_Premultiplied);
    merged.fill(QColor(Qt::transparent));

    QPainter painter(&merged);
    for (int i = 0; i < state_.layers.size(); ++i) {
        const Layer &layer = state_.layers[i];
        if (!layer.visible || layer.image.isNull()) {
            continue;
        }
        const bool implicitBackground = i == 0
            && !state_.transparentBackground
            && layer.name.compare(QStringLiteral("Background"), Qt::CaseInsensitive) == 0;
        if (!includeBackground && implicitBackground) {
            continue;
        }

        if (layer.blendMode == QStringLiteral("Multiply")) {
            painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        } else if (layer.blendMode == QStringLiteral("Screen")) {
            painter.setCompositionMode(QPainter::CompositionMode_Screen);
        } else if (layer.blendMode == QStringLiteral("Overlay")) {
            painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        } else {
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        painter.setOpacity(std::clamp(layer.opacity, 0.0, 1.0));
        painter.drawImage(QPoint(0, 0), layer.image);
    }
    painter.setOpacity(1.0);
    cache = merged;
    cacheValid = true;
    return cache;
}

DocumentState CanvasDocument::snapshot() const
{
    return deepCopyStateImages(state_);
}

void CanvasDocument::restore(const DocumentState &state)
{
    invalidateCompositeCache();
    state_ = deepCopyStateImages(state);
}

void CanvasDocument::setFilePath(const QString &filePath)
{
    state_.filePath = filePath;
}

void CanvasDocument::setModified(bool modified)
{
    state_.modified = modified;
    if (modified) {
        invalidateCompositeCache();
    }
}

void CanvasDocument::setActiveLayerIndex(int index)
{
    if (validLayerIndex(index)) {
        state_.activeLayerIndex = index;
    }
}

int CanvasDocument::addLayer(const QString &name)
{
    if (!isValid()) {
        return -1;
    }
    const QString layerName = name.isEmpty() ? QStringLiteral("Layer %1").arg(state_.layers.size() + 1) : name;
    state_.layers.push_back(makeLayer(size(), layerName));
    state_.activeLayerIndex = state_.layers.size() - 1;
    state_.modified = true;
    invalidateCompositeCache();
    return state_.activeLayerIndex;
}

bool CanvasDocument::deleteLayer(int index)
{
    if (!validLayerIndex(index) || state_.layers.size() <= 1) {
        return false;
    }
    const int previousActive = state_.activeLayerIndex;
    state_.layers.removeAt(index);
    if (previousActive == index) {
        state_.activeLayerIndex = std::min(index, static_cast<int>(state_.layers.size()) - 1);
    } else if (previousActive > index) {
        state_.activeLayerIndex = previousActive - 1;
    } else {
        state_.activeLayerIndex = previousActive;
    }
    state_.activeLayerIndex = std::clamp(state_.activeLayerIndex, 0, static_cast<int>(state_.layers.size()) - 1);
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

int CanvasDocument::duplicateLayer(int index)
{
    if (!validLayerIndex(index)) {
        return -1;
    }
    Layer copy = state_.layers[index];
    copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.name += QStringLiteral(" Copy");
    state_.layers.insert(index + 1, copy);
    state_.activeLayerIndex = index + 1;
    state_.modified = true;
    invalidateCompositeCache();
    return state_.activeLayerIndex;
}

bool CanvasDocument::moveLayer(int from, int to)
{
    if (!validLayerIndex(from) || !validLayerIndex(to) || from == to) {
        return false;
    }
    state_.layers.move(from, to);
    state_.activeLayerIndex = to;
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

bool CanvasDocument::mergeLayerDown(int index)
{
    if (!validLayerIndex(index) || index <= 0) {
        return false;
    }

    Layer &below = state_.layers[index - 1];
    const Layer top = state_.layers[index];
    if (top.visible && top.opacity > 0.0) {
        QPainter painter(&below.image);
        if (top.blendMode == QStringLiteral("Multiply")) {
            painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        } else if (top.blendMode == QStringLiteral("Screen")) {
            painter.setCompositionMode(QPainter::CompositionMode_Screen);
        } else if (top.blendMode == QStringLiteral("Overlay")) {
            painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        } else {
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }
        painter.setOpacity(std::clamp(top.opacity, 0.0, 1.0));
        painter.drawImage(QPoint(0, 0), top.image);
    }
    state_.layers.removeAt(index);
    state_.activeLayerIndex = index - 1;
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

bool CanvasDocument::renameLayer(int index, const QString &name)
{
    if (!validLayerIndex(index) || name.trimmed().isEmpty()) {
        return false;
    }
    const QString trimmed = name.trimmed();
    if (state_.layers[index].name == trimmed) {
        return false;
    }
    state_.layers[index].name = trimmed;
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

bool CanvasDocument::setLayerVisible(int index, bool visible)
{
    if (!validLayerIndex(index)) {
        return false;
    }
    if (state_.layers[index].visible == visible) {
        return false;
    }
    state_.layers[index].visible = visible;
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

bool CanvasDocument::setLayerLocked(int index, bool locked)
{
    if (!validLayerIndex(index)) {
        return false;
    }
    if (state_.layers[index].locked == locked) {
        return false;
    }
    state_.layers[index].locked = locked;
    state_.modified = true;
    return true;
}

bool CanvasDocument::setLayerAlphaLock(int index, bool alphaLock)
{
    if (!validLayerIndex(index)) {
        return false;
    }
    if (state_.layers[index].alphaLock == alphaLock) {
        return false;
    }
    state_.layers[index].alphaLock = alphaLock;
    state_.modified = true;
    return true;
}

bool CanvasDocument::setLayerOpacity(int index, double opacity)
{
    if (!validLayerIndex(index)) {
        return false;
    }
    const double clamped = std::clamp(opacity, 0.0, 1.0);
    if (std::abs(state_.layers[index].opacity - clamped) <= 0.000001) {
        return false;
    }
    state_.layers[index].opacity = clamped;
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

bool CanvasDocument::setLayerBlendMode(int index, const QString &blendMode)
{
    if (!validLayerIndex(index)) {
        return false;
    }
    if (state_.layers[index].blendMode == blendMode) {
        return false;
    }
    state_.layers[index].blendMode = blendMode;
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

bool CanvasDocument::clearLayer(int index, const QRect &rect)
{
    if (!validLayerIndex(index)) {
        return false;
    }

    QImage &image = state_.layers[index].image;
    if (image.isNull()) {
        return false;
    }

    const QRect targetRect = rect.isValid()
        ? rect.intersected(QRect(QPoint(0, 0), image.size()))
        : QRect(QPoint(0, 0), image.size());
    if (!targetRect.isValid()) {
        return false;
    }

    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(targetRect, Qt::transparent);
    state_.modified = true;
    invalidateCompositeCache();
    return true;
}

Layer CanvasDocument::makeLayer(const QSize &size, const QString &name, const QColor &fill)
{
    Layer layer;
    layer.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    layer.name = name;
    layer.image = QImage(size, QImage::Format_ARGB32_Premultiplied);
    layer.image.fill(fill);
    return layer;
}

bool CanvasDocument::validLayerIndex(int index) const
{
    return index >= 0 && index < state_.layers.size();
}

void CanvasDocument::invalidateCompositeCache() const
{
    compositeWithBackgroundValid_ = false;
    compositeWithoutBackgroundValid_ = false;
}

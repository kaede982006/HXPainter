#include "OpenGLCanvasWidget.h"

#include "commands/Command.h"
#include "brush/TextEngine.h"
#include "input/TabletInputMapper.h"
#include "ui/TextSettingsDialog.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QTabletEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace {
QRect normalizedCanvasRect(const QPointF &a, const QPointF &b, const QSize &bounds)
{
    return QRectF(a, b).normalized().toAlignedRect().intersected(QRect(QPoint(0, 0), bounds));
}

void restoreAlphaInRect(QImage &target, const QImage &alphaSource, QRect dirty)
{
    dirty = dirty.intersected(QRect(QPoint(0, 0), target.size()));
    if (!dirty.isValid() || alphaSource.size() != target.size()) {
        return;
    }

    QImage working = target.convertToFormat(QImage::Format_ARGB32);
    QImage alpha = alphaSource.convertToFormat(QImage::Format_ARGB32);
    for (int y = dirty.top(); y <= dirty.bottom(); ++y) {
        for (int x = dirty.left(); x <= dirty.right(); ++x) {
            QColor color = QColor::fromRgba(working.pixel(x, y));
            color.setAlpha(qAlpha(alpha.pixel(x, y)));
            working.setPixelColor(x, y, color);
        }
    }
    target = working.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QColor colorWithFillOpacity(const QColor &source, double opacity)
{
    QColor color = source;
    color.setAlphaF(std::clamp(opacity, 0.0, 1.0) * source.alphaF());
    return color;
}

double radiansToDegrees(double radians)
{
    return radians * 180.0 / 3.14159265358979323846;
}

QPolygonF polygonForRect(const QRectF &rect)
{
    QPolygonF polygon;
    if (!rect.isValid()) {
        return polygon;
    }
    polygon << rect.topLeft()
            << rect.topRight()
            << rect.bottomRight()
            << rect.bottomLeft();
    return polygon;
}

QPolygonF translatedPolygon(const QPolygonF &polygon, const QPointF &delta)
{
    QPolygonF translated;
    translated.reserve(polygon.size());
    for (const QPointF &point : polygon) {
        translated << point + delta;
    }
    return translated;
}

QPointF polygonCenter(const QPolygonF &polygon)
{
    if (polygon.isEmpty()) {
        return {};
    }
    QPointF sum;
    for (const QPointF &point : polygon) {
        sum += point;
    }
    return sum / polygon.size();
}

QPointF midpoint(const QPointF &a, const QPointF &b)
{
    return (a + b) * 0.5;
}

QVector<QPointF> transformHandlesForPolygon(const QPolygonF &polygon)
{
    if (polygon.size() < 4) {
        return {};
    }
    return {
        polygon[0],
        midpoint(polygon[0], polygon[1]),
        polygon[1],
        midpoint(polygon[1], polygon[2]),
        polygon[2],
        midpoint(polygon[2], polygon[3]),
        polygon[3],
        midpoint(polygon[3], polygon[0]),
    };
}

QPointF rotationHandleForPolygon(const QPolygonF &polygon, double handleRadius)
{
    if (polygon.size() < 2) {
        return {};
    }
    const QPointF topMiddle = midpoint(polygon[0], polygon[1]);
    const QPointF topEdge = polygon[1] - polygon[0];
    const double length = std::max(1.0, std::hypot(topEdge.x(), topEdge.y()));
    const QPointF outward(topEdge.y() / length, -topEdge.x() / length);
    return topMiddle + outward * handleRadius * 2.75;
}

QRect polygonBounds(const QPolygonF &polygon, const QSize &canvasSize)
{
    if (polygon.isEmpty()) {
        return {};
    }
    return polygon.boundingRect().toAlignedRect().intersected(QRect(QPoint(0, 0), canvasSize));
}

int rectArea(const QRect &rect)
{
    return rect.isValid() ? rect.width() * rect.height() : 0;
}

QRect subtractAsSingleRect(const QRect &base, const QRect &cut)
{
    if (!base.isValid() || !cut.isValid() || !base.intersects(cut)) {
        return base;
    }

    const QRect intersection = base.intersected(cut);
    QVector<QRect> candidates = {
        QRect(QPoint(base.left(), base.top()), QPoint(intersection.left() - 1, base.bottom())),
        QRect(QPoint(intersection.right() + 1, base.top()), QPoint(base.right(), base.bottom())),
        QRect(QPoint(intersection.left(), base.top()), QPoint(intersection.right(), intersection.top() - 1)),
        QRect(QPoint(intersection.left(), intersection.bottom() + 1), QPoint(intersection.right(), base.bottom())),
    };

    QRect best;
    for (const QRect &candidate : std::as_const(candidates)) {
        if (rectArea(candidate) > rectArea(best)) {
            best = candidate;
        }
    }
    return best;
}
}

OpenGLCanvasWidget::OpenGLCanvasWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TabletTracking, true);
    setAttribute(Qt::WA_AcceptTouchEvents, true);

    layerManager_ = new LayerManager(&document_, this);
    layerManager_->setCommandManager(&commandManager_);

    QObject::connect(&commandManager_, &CommandManager::stackChanged, this, [this] {
        layerManager_->notifyDocumentReset();
        notifyChanged();
    });
    QObject::connect(layerManager_, &LayerManager::layersChanged, this, [this] {
        notifyChanged();
    });
    QObject::connect(layerManager_, &LayerManager::statusMessage, this, &OpenGLCanvasWidget::statusMessage);

    inputClock_.start();
    centerCanvas();
}

void OpenGLCanvasWidget::setToolManager(ToolManager *toolManager)
{
    toolManager_ = toolManager;
    if (toolManager_) {
        QObject::connect(toolManager_, &ToolManager::activeToolChanged, this, [this](ToolType type) {
            if (drawing_) {
                finishStroke();
            }
            if (Tool *tool = toolManager_->tool(type)) {
                setCursor(tool->cursor());
            }
            Q_EMIT statusMessage(QStringLiteral("Tool: %1").arg(toolTypeName(type)));
            notifyChanged();
        });
    }
}

void OpenGLCanvasWidget::setBrushColor(const QColor &color)
{
    setForegroundColor(color);
}

void OpenGLCanvasWidget::setForegroundColor(const QColor &color)
{
    foregroundColor_ = color;
    brushSettings_.color = color;
    notifyChanged();
}

void OpenGLCanvasWidget::setBackgroundColor(const QColor &color)
{
    backgroundColor_ = color;
    notifyChanged();
}

void OpenGLCanvasWidget::setBrushRadius(double radius)
{
    brushSettings_.radius = std::clamp(radius, 1.0, 512.0);
    notifyChanged();
}

void OpenGLCanvasWidget::setBrushOpacity(double opacity)
{
    brushSettings_.opacity = std::clamp(opacity, 0.01, 1.0);
    notifyChanged();
}

void OpenGLCanvasWidget::setPressureControlsRadius(bool enabled)
{
    brushSettings_.pressureControlsRadius = enabled;
}

void OpenGLCanvasWidget::setPressureControlsOpacity(bool enabled)
{
    brushSettings_.pressureControlsOpacity = enabled;
}

void OpenGLCanvasWidget::setBrushSettings(const BrushSettings &settings)
{
    brushSettings_ = settings;
    brushSettings_.color = foregroundColor_;
    notifyChanged();
}

void OpenGLCanvasWidget::setEraserSettings(const EraserSettings &settings)
{
    eraserSettings_ = settings;
    eraserSettings_.eraseCurrentLayerOnly = true;
    notifyChanged();
}

void OpenGLCanvasWidget::setFillSettings(const FillSettings &settings)
{
    fillSettings_ = settings;
    notifyChanged();
}

void OpenGLCanvasWidget::setTextSettings(const TextSettings &settings)
{
    textSettings_ = settings;
    if (textSettings_.font.pointSize() <= 0) {
        textSettings_.font.setPointSize(32);
    }
    if (textSettings_.boxSize.width() < 32.0 || textSettings_.boxSize.height() < 32.0) {
        textSettings_.boxSize = QSizeF(480.0, 220.0);
    }
    notifyChanged();
}

void OpenGLCanvasWidget::setSelectionSettings(const QString &mode, int feather, bool antiAlias)
{
    selectionMode_ = mode;
    selectionFeather_ = std::clamp(feather, 0, 256);
    selectionAntiAlias_ = antiAlias;
}

void OpenGLCanvasWidget::setShapeSettings(int strokeWidth, bool fillEnabled)
{
    shapeStrokeWidth_ = std::clamp(strokeWidth, 1, 256);
    shapeFillEnabled_ = fillEnabled;
}

QColor OpenGLCanvasWidget::brushColor() const
{
    return foregroundColor_;
}

QColor OpenGLCanvasWidget::foregroundColor() const
{
    return foregroundColor_;
}

QColor OpenGLCanvasWidget::backgroundColor() const
{
    return backgroundColor_;
}

BrushSettings OpenGLCanvasWidget::brushSettings() const
{
    return brushSettings_;
}

EraserSettings OpenGLCanvasWidget::eraserSettings() const
{
    return eraserSettings_;
}

FillSettings OpenGLCanvasWidget::fillSettings() const
{
    return fillSettings_;
}

TextSettings OpenGLCanvasWidget::textSettings() const
{
    return textSettings_;
}

StatsSnapshot OpenGLCanvasWidget::statsSnapshot() const
{
    return stats_.snapshot();
}

const CanvasDocument &OpenGLCanvasWidget::document() const
{
    return document_;
}

CanvasDocument &OpenGLCanvasWidget::document()
{
    return document_;
}

CommandManager *OpenGLCanvasWidget::commandManager()
{
    return &commandManager_;
}

LayerManager *OpenGLCanvasWidget::layerManager()
{
    return layerManager_;
}

bool OpenGLCanvasWidget::hasDocument() const
{
    return document_.isValid();
}

double OpenGLCanvasWidget::zoom() const
{
    return zoom_;
}

bool OpenGLCanvasWidget::hasSelection() const
{
    return selectionRect_.isValid() && selectionRect_.width() > 0 && selectionRect_.height() > 0;
}

QRect OpenGLCanvasWidget::selectionRect() const
{
    return selectionRect_;
}

void OpenGLCanvasWidget::newDocument(const NewDocumentSettings &settings)
{
    resetInteractionState();
    document_.reset(settings);
    commandManager_.clear();
    stats_.reset();
    zoom_ = 1.0;
    rotationDegrees_ = 0.0;
    clearSelection();
    centerCanvas();
    layerManager_->notifyDocumentReset();
    notifyChanged();
}

void OpenGLCanvasWidget::newDocument(const QSize &size)
{
    NewDocumentSettings settings;
    settings.width = size.width();
    settings.height = size.height();
    newDocument(settings);
}

bool OpenGLCanvasWidget::openImage(const QString &filePath)
{
    if (!document_.loadImageAsDocument(filePath)) {
        return false;
    }
    notifyDocumentLoaded();
    return true;
}

bool OpenGLCanvasWidget::saveImage(const QString &filePath) const
{
    return document_.exportCompositedImage(filePath);
}

void OpenGLCanvasWidget::restoreDocument(const DocumentState &state)
{
    document_.restore(state);
    notifyDocumentLoaded(state.filePath);
}

void OpenGLCanvasWidget::notifyDocumentLoaded(const QString &filePath)
{
    resetInteractionState();
    if (!filePath.isEmpty()) {
        document_.setFilePath(filePath);
    }
    commandManager_.clear();
    stats_.reset();
    zoom_ = 1.0;
    rotationDegrees_ = 0.0;
    clearSelection();
    centerCanvas();
    layerManager_->notifyDocumentReset();
    notifyChanged();
}

void OpenGLCanvasWidget::zoomIn()
{
    zoomAt(QPointF(width() / 2.0, height() / 2.0), 1.2);
}

void OpenGLCanvasWidget::zoomOut()
{
    zoomAt(QPointF(width() / 2.0, height() / 2.0), 1.0 / 1.2);
}

void OpenGLCanvasWidget::fitCanvas()
{
    if (!document_.isValid() || width() <= 0 || height() <= 0) {
        return;
    }
    const double sx = width() / static_cast<double>(document_.size().width());
    const double sy = height() / static_cast<double>(document_.size().height());
    zoom_ = std::clamp(std::min(sx, sy) * 0.92, 0.05, 16.0);
    centerCanvas();
    update();
    Q_EMIT zoomChanged(zoom_);
}

void OpenGLCanvasWidget::resetView()
{
    rotationDegrees_ = 0.0;
    zoom_ = 1.0;
    centerCanvas();
    update();
    Q_EMIT zoomChanged(zoom_);
}

void OpenGLCanvasWidget::rotateViewLeft()
{
    rotationDegrees_ = std::fmod(rotationDegrees_ - 15.0 + 360.0, 360.0);
    update();
    Q_EMIT statusMessage(QStringLiteral("View rotation: %1 deg").arg(rotationDegrees_, 0, 'f', 0));
}

void OpenGLCanvasWidget::rotateViewRight()
{
    rotationDegrees_ = std::fmod(rotationDegrees_ + 15.0, 360.0);
    update();
    Q_EMIT statusMessage(QStringLiteral("View rotation: %1 deg").arg(rotationDegrees_, 0, 'f', 0));
}

void OpenGLCanvasWidget::setSelectionRect(const QRect &selection)
{
    selectionRect_ = selection.intersected(QRect(QPoint(0, 0), document_.size()));
    selectionPolygon_ = polygonForRect(QRectF(selectionRect_));
    selecting_ = false;
    update();
    Q_EMIT selectionChanged(selectionRect_);
}

void OpenGLCanvasWidget::clearSelection()
{
    selecting_ = false;
    selectionRect_ = QRect();
    selectionPolygon_.clear();
    update();
    Q_EMIT selectionChanged(selectionRect_);
}

void OpenGLCanvasWidget::transformSelection(double rotationDegrees, int targetWidth, int targetHeight, int scalePercent)
{
    if (!activeLayerWritable()) {
        return;
    }
    if (!hasSelection()) {
        Q_EMIT statusMessage(QStringLiteral("Create a selection before transforming it"));
        return;
    }

    const QRect canvasBounds(QPoint(0, 0), document_.size());
    const QRect sourceRect = selectionRect_.intersected(canvasBounds);
    if (!sourceRect.isValid() || sourceRect.width() < 1 || sourceRect.height() < 1) {
        Q_EMIT statusMessage(QStringLiteral("Selection is empty"));
        return;
    }

    const DocumentState before = document_.snapshot();
    if (before.activeLayerIndex < 0 || before.activeLayerIndex >= before.layers.size()) {
        Q_EMIT statusMessage(QStringLiteral("No active layer for selection transform"));
        return;
    }
    if (before.layers[before.activeLayerIndex].image.isNull()) {
        Q_EMIT statusMessage(QStringLiteral("No active layer image for selection transform"));
        return;
    }

    const QImage source = before.layers[before.activeLayerIndex].image.copy(sourceRect);
    const double uniformScale = std::clamp(scalePercent, 1, 1000) / 100.0;
    const QSize baseSize(std::max(1, targetWidth), std::max(1, targetHeight));
    const QSize scaledSize(std::max(1, static_cast<int>(std::round(baseSize.width() * uniformScale))),
                           std::max(1, static_cast<int>(std::round(baseSize.height() * uniformScale))));

    DocumentState after;
    QRect visibleDestination;
    QPolygonF destinationPolygon;
    if (!buildSelectionTransformState(before,
                                      sourceRect,
                                      source,
                                      QRectF(sourceRect).center(),
                                      scaledSize,
                                      rotationDegrees,
                                      &after,
                                      &visibleDestination,
                                      &destinationPolygon)) {
        Q_EMIT statusMessage(QStringLiteral("Selection transform produced no visible pixels"));
        return;
    }

    const QRect beforeSelection = selectionRect_;
    const QPolygonF beforePolygon = selectionPolygon_.isEmpty() ? polygonForRect(QRectF(beforeSelection)) : selectionPolygon_;
    const QRect afterSelection = visibleDestination;
    const QPolygonF afterPolygon = destinationPolygon;
    commandManager_.execute(std::make_unique<FunctionalCommand>(
        QStringLiteral("Transform Selection"),
        [this, before, beforeSelection, beforePolygon] {
            document_.restore(before);
            selectionRect_ = beforeSelection;
            selectionPolygon_ = beforePolygon;
            update();
            Q_EMIT selectionChanged(selectionRect_);
        },
        [this, after, afterSelection, afterPolygon] {
            document_.restore(after);
            selectionRect_ = afterSelection;
            selectionPolygon_ = afterPolygon;
            update();
            Q_EMIT selectionChanged(selectionRect_);
        }));
    selectionRect_ = afterSelection;
    selectionPolygon_ = afterPolygon;
    update();
    Q_EMIT selectionChanged(selectionRect_);
    Q_EMIT statusMessage(QStringLiteral("Selection transformed: %1 x %2, %3 deg")
        .arg(selectionRect_.width())
        .arg(selectionRect_.height())
        .arg(rotationDegrees, 0, 'f', 1));
}

void OpenGLCanvasWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QElapsedTimer timer;
    timer.start();

    QPainter painter(this);
    painter.fillRect(rect(), QColor(34, 36, 40));

    if (!document_.isValid()) {
        painter.setPen(QColor(230, 232, 236));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Create or open a document to start painting"));
        stats_.recordFrame(timer.nsecsElapsed() / 1'000'000.0);
        return;
    }

    painter.save();
    painter.setTransform(canvasTransform(), true);
    const QRectF canvasRect(QPointF(0, 0), QSizeF(document_.size()));
    drawCheckerboard(painter, canvasRect);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 3.0);
    painter.drawImage(QPointF(0, 0), document_.compositedImage());

    QPen borderPen(QColor(0, 0, 0, 160), 0);
    borderPen.setCosmetic(true);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(canvasRect.adjusted(0, 0, -1, -1));

    if (hasSelection()) {
        painter.setRenderHint(QPainter::Antialiasing, selectionAntiAlias_);
        QPen selectionPen(QColor(77, 156, 255), 0, Qt::DashLine);
        selectionPen.setCosmetic(true);
        painter.setPen(selectionPen);
        painter.setBrush(QColor(77, 156, 255, 24));
        const QPolygonF displayPolygon = selectionPolygon_.isEmpty()
            ? polygonForRect(QRectF(selectionRect_))
            : selectionPolygon_;
        if (displayPolygon.size() >= 4) {
            painter.drawPolygon(displayPolygon);
        } else {
            painter.drawRect(selectionRect_);
        }

        const bool showTransformHandles = toolManager_ && toolManager_->activeToolType() == ToolType::Transform;
        if (showTransformHandles && displayPolygon.size() >= 4) {
            const double handleSize = std::max(7.0, 14.0 / std::max(zoom_, 0.05));
            const QVector<QPointF> points = transformHandlesForPolygon(displayPolygon);
            painter.setPen(QPen(QColor(23, 32, 44), 0));
            painter.setBrush(QColor(245, 248, 252));
            for (const QPointF &point : points) {
                painter.drawRect(QRectF(point.x() - handleSize * 0.5,
                                        point.y() - handleSize * 0.5,
                                        handleSize,
                                        handleSize));
            }
            const QPointF topMiddle = midpoint(displayPolygon[0], displayPolygon[1]);
            const QPointF rotatePoint = rotationHandleForPolygon(displayPolygon, handleSize);
            painter.drawLine(topMiddle, rotatePoint);
            painter.setBrush(QColor(77, 156, 255));
            painter.drawEllipse(rotatePoint, handleSize * 0.55, handleSize * 0.55);
        }
    }

    if (shaping_ && shapePreviewRect_.isValid()) {
        QPen shapePen(foregroundColor_, 0, Qt::DashLine);
        shapePen.setCosmetic(true);
        painter.setPen(shapePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(shapePreviewRect_);
    }
    painter.restore();

    stats_.recordFrame(timer.nsecsElapsed() / 1'000'000.0);
}

void OpenGLCanvasWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    if (pan_.isNull()) {
        centerCanvas();
    }
}

void OpenGLCanvasWidget::tabletEvent(QTabletEvent *event)
{
    if (event->type() == QEvent::TabletPress) {
        setFocus(Qt::MouseFocusReason);
    }
    lastTabletEventMs_ = inputClock_.elapsed();
    lastTabletEventWidgetPosition_ = event->position();
    const QPointF canvasPosition = widgetToCanvas(event->position());
    Q_EMIT cursorPositionChanged(canvasPosition);
    TabletInputEvent mapped = TabletInputMapper::fromTabletEvent(*event, canvasPosition);
    const ToolType active = toolManager_ ? toolManager_->activeToolType() : ToolType::Brush;
    if (active == ToolType::Text && mapped.kind == InputEventKind::Press && mapped.isPrimaryDown()) {
        const qint64 now = inputClock_.elapsed();
        const double distance = std::hypot(event->position().x() - lastTabletTextPressWidget_.x(),
                                          event->position().y() - lastTabletTextPressWidget_.y());
        if (now - lastTabletTextPressMs_ >= 0 && now - lastTabletTextPressMs_ <= 500 && distance <= 24.0) {
            lastTabletTextPressMs_ = -1000;
            insertText(mapped);
        } else {
            lastTabletTextPressMs_ = now;
            lastTabletTextPressWidget_ = event->position();
            Q_EMIT statusMessage(QStringLiteral("Double-tap the canvas to insert text with font, size, and color settings"));
        }
        event->accept();
        return;
    }
    handleInput(mapped);
    event->accept();
}

void OpenGLCanvasWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        setFocus(Qt::OtherFocusReason);
    }
    if (shouldSuppressSynthesizedMouseEvent(*event)) {
        event->accept();
        return;
    }

    const ToolType type = toolManager_ ? toolManager_->activeToolType() : ToolType::Brush;
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton || type == ToolType::Hand) {
        panning_ = true;
        lastPanPosition_ = event->position();
        event->accept();
        return;
    }

    const QPointF canvasPosition = widgetToCanvas(event->position());
    Q_EMIT cursorPositionChanged(canvasPosition);
    TabletInputEvent mapped = TabletInputMapper::fromMouseEvent(*event, canvasPosition);
    handleInput(mapped);
    event->accept();
}

void OpenGLCanvasWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    const ToolType type = toolManager_ ? toolManager_->activeToolType() : ToolType::Brush;
    if (type == ToolType::Text && event->button() == Qt::LeftButton) {
        const QPointF canvasPosition = widgetToCanvas(event->position());
        Q_EMIT cursorPositionChanged(canvasPosition);
        TabletInputEvent mapped = TabletInputMapper::fromMouseEvent(*event, canvasPosition);
        mapped.kind = InputEventKind::Press;
        mapped.buttons |= Qt::LeftButton;
        insertText(mapped);
        event->accept();
        return;
    }

    mousePressEvent(event);
}

void OpenGLCanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (shouldSuppressSynthesizedMouseEvent(*event)) {
        event->accept();
        return;
    }

    if (panning_) {
        const QPointF delta = event->position() - lastPanPosition_;
        pan_ += delta;
        lastPanPosition_ = event->position();
        update();
        event->accept();
        return;
    }

    const QPointF canvasPosition = widgetToCanvas(event->position());
    Q_EMIT cursorPositionChanged(canvasPosition);
    TabletInputEvent mapped = TabletInputMapper::fromMouseEvent(*event, canvasPosition);
    handleInput(mapped);
    event->accept();
}

void OpenGLCanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (shouldSuppressSynthesizedMouseEvent(*event)) {
        event->accept();
        return;
    }

    if (panning_) {
        panning_ = false;
        event->accept();
        return;
    }

    const QPointF canvasPosition = widgetToCanvas(event->position());
    Q_EMIT cursorPositionChanged(canvasPosition);
    TabletInputEvent mapped = TabletInputMapper::fromMouseEvent(*event, canvasPosition);
    handleInput(mapped);
    event->accept();
}

void OpenGLCanvasWidget::wheelEvent(QWheelEvent *event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (std::abs(steps) > 0.001) {
        zoomAt(event->position(), std::pow(1.15, steps));
    }
    event->accept();
}

QPointF OpenGLCanvasWidget::canvasToWidget(const QPointF &canvasPosition) const
{
    return canvasTransform().map(canvasPosition);
}

QPointF OpenGLCanvasWidget::widgetToCanvas(const QPointF &widgetPosition) const
{
    bool invertible = false;
    const QTransform inverse = canvasTransform().inverted(&invertible);
    return invertible ? inverse.map(widgetPosition) : (widgetPosition - pan_) / zoom_;
}

QTransform OpenGLCanvasWidget::canvasTransform() const
{
    QTransform transform;
    if (!document_.isValid()) {
        transform.translate(pan_.x(), pan_.y());
        transform.scale(zoom_, zoom_);
        return transform;
    }

    const QPointF center(document_.size().width() * 0.5, document_.size().height() * 0.5);
    transform.translate(pan_.x(), pan_.y());
    transform.translate(center.x() * zoom_, center.y() * zoom_);
    transform.rotate(rotationDegrees_);
    transform.translate(-center.x() * zoom_, -center.y() * zoom_);
    transform.scale(zoom_, zoom_);
    return transform;
}

void OpenGLCanvasWidget::centerCanvas()
{
    if (!document_.isValid() || width() <= 0 || height() <= 0) {
        pan_ = QPointF(48.0, 48.0);
        return;
    }
    const QSizeF scaled = QSizeF(document_.size()) * zoom_;
    pan_ = QPointF((width() - scaled.width()) * 0.5, (height() - scaled.height()) * 0.5);
}

void OpenGLCanvasWidget::drawCheckerboard(QPainter &painter, const QRectF &canvasRect)
{
    painter.save();
    painter.setClipRect(canvasRect);
    const int cell = 16;
    const QColor light(218, 220, 224);
    const QColor dark(196, 198, 202);
    for (int y = static_cast<int>(canvasRect.top()); y < static_cast<int>(canvasRect.bottom()); y += cell) {
        for (int x = static_cast<int>(canvasRect.left()); x < static_cast<int>(canvasRect.right()); x += cell) {
            const bool alternate = ((x / cell) + (y / cell)) % 2 == 0;
            painter.fillRect(QRect(x, y, cell, cell), alternate ? light : dark);
        }
    }
    painter.restore();
}

void OpenGLCanvasWidget::handleInput(const TabletInputEvent &event)
{
    stats_.recordInput(event);
    if (!document_.isValid()) {
        if (event.kind == InputEventKind::Press) {
            Q_EMIT statusMessage(QStringLiteral("Create or open a document before using tools"));
        }
        return;
    }

    ToolType active = toolManager_ ? toolManager_->activeToolType() : ToolType::Brush;
    if (event.device == InputDeviceKind::Eraser) {
        active = ToolType::Eraser;
    }
    if (drawing_ && event.kind == InputEventKind::Press && shouldIgnoreDuplicateStrokePress(event, active)) {
        return;
    }
    if (drawing_ && active != strokeTool_) {
        finishStroke();
    }
    if (event.kind == InputEventKind::Press && drawing_) {
        finishStroke();
    }
    if (active == ToolType::Selection) {
        handleSelection(event);
        return;
    }
    if (active == ToolType::Transform) {
        handleTransformTool(event);
        return;
    }
    if (active == ToolType::Shape) {
        handleShape(event);
        return;
    }
    if (active == ToolType::Move) {
        handleMoveTool(event);
        return;
    }
    if (event.kind == InputEventKind::Release) {
        finishStroke();
        return;
    }

    if (active == ToolType::Fill && event.kind == InputEventKind::Press && event.isPrimaryDown()) {
        renderFill(event);
        return;
    }
    if (active == ToolType::ColorPicker && event.kind == InputEventKind::Press) {
        pickColor(event);
        return;
    }
    if (active == ToolType::Text && event.kind == InputEventKind::Press && event.isPrimaryDown()) {
        Q_EMIT statusMessage(QStringLiteral("Double-click the canvas to insert text with font, size, and color settings"));
        return;
    }
    if (active == ToolType::Zoom && event.kind == InputEventKind::Press) {
        zoomAt(event.widgetPosition, event.modifiers.testFlag(Qt::AltModifier) ? 1.0 / 1.2 : 1.2);
        return;
    }

    if ((event.kind == InputEventKind::Press || event.kind == InputEventKind::Move) && event.isPrimaryDown()) {
        if (active == ToolType::Brush) {
            renderBrushStroke(event);
        } else if (active == ToolType::Eraser) {
            renderEraserStroke(event);
        }
    }
}

void OpenGLCanvasWidget::renderBrushStroke(const TabletInputEvent &event)
{
    if (!activeLayerWritable()) {
        return;
    }
    QImage *image = document_.activeImage();
    if (!image) {
        Q_EMIT statusMessage(QStringLiteral("No active layer image for brush"));
        return;
    }

    const QRect imageBounds(QPoint(0, 0), document_.size());
    if (!imageBounds.adjusted(-512, -512, 512, 512).contains(event.canvasPosition.toPoint())) {
        if (event.kind == InputEventKind::Press) {
            Q_EMIT statusMessage(QStringLiteral("Brush position is outside the canvas"));
        }
        return;
    }

    QElapsedTimer timer;
    timer.start();
    StrokeSample sample{event.canvasPosition, std::clamp(event.pressure, 0.0, 1.0), event.timestampNs};

    const bool alphaLocked = document_.activeLayer() && document_.activeLayer()->alphaLock;
    const QImage alphaBefore = alphaLocked ? image->copy() : QImage();
    BrushRenderResult result;
    if (!drawing_) {
        strokeBefore_ = document_.snapshot();
        strokeTool_ = ToolType::Brush;
        strokeDevice_ = event.device;
        drawing_ = true;
        strokeChanged_ = false;
        result = brushEngine_.beginStroke(*image, sample, brushSettings_);
    } else {
        result = brushEngine_.continueStroke(*image, sample, brushSettings_);
    }
    strokeLastCanvasPosition_ = event.canvasPosition;
    strokeHasLastCanvasPosition_ = true;
    if (alphaLocked) {
        restoreAlphaInRect(*image, alphaBefore, result.dirtyRect);
    }
    strokeChanged_ = strokeChanged_ || result.hasChanges();

    stats_.recordStroke(timer.nsecsElapsed() / 1'000'000.0, result.dirtyRect, result.samples);
    if (result.hasChanges()) {
        document_.setModified(true);
        update();
    }
}

void OpenGLCanvasWidget::renderEraserStroke(const TabletInputEvent &event)
{
    if (!activeLayerWritable()) {
        return;
    }
    if (eraserSettings_.eraseAlphaOnly && document_.activeLayer() && document_.activeLayer()->alphaLock) {
        Q_EMIT statusMessage(QStringLiteral("Alpha lock prevents erasing layer alpha"));
        return;
    }
    QImage *image = document_.activeImage();
    if (!image) {
        Q_EMIT statusMessage(QStringLiteral("No active layer image for eraser"));
        return;
    }

    QElapsedTimer timer;
    timer.start();
    StrokeSample sample{event.canvasPosition, std::clamp(event.pressure, 0.0, 1.0), event.timestampNs};
    QRect dirty;
    int samples = 0;
    bool changed = false;

    if (!drawing_) {
        strokeBefore_ = document_.snapshot();
        strokeTool_ = ToolType::Eraser;
        strokeDevice_ = event.device;
        drawing_ = true;
        strokeChanged_ = false;
    }

    if (eraserSettings_.eraseAlphaOnly) {
        const EraserRenderResult result = !strokeChanged_
            ? eraserEngine_.beginStroke(*image, sample, eraserSettings_)
            : eraserEngine_.continueStroke(*image, sample, eraserSettings_);
        dirty = result.dirtyRect;
        samples = result.samples;
        changed = result.hasChanges();
    } else {
        BrushSettings paintSettings;
        paintSettings.color = backgroundColor_;
        paintSettings.radius = eraserSettings_.radius;
        paintSettings.opacity = eraserSettings_.opacity;
        paintSettings.flow = 1.0;
        paintSettings.hardness = 1.0;
        paintSettings.spacing = eraserSettings_.spacing;
        paintSettings.pressureControlsRadius = eraserSettings_.pressureControlsRadius;
        paintSettings.pressureControlsOpacity = false;

        const bool alphaLocked = document_.activeLayer() && document_.activeLayer()->alphaLock;
        const QImage alphaBefore = alphaLocked ? image->copy() : QImage();
        const BrushRenderResult result = !strokeChanged_
            ? brushEngine_.beginStroke(*image, sample, paintSettings)
            : brushEngine_.continueStroke(*image, sample, paintSettings);
        dirty = result.dirtyRect;
        samples = result.samples;
        changed = result.hasChanges();
        if (alphaLocked) {
            restoreAlphaInRect(*image, alphaBefore, dirty);
        }
    }
    strokeChanged_ = strokeChanged_ || changed;
    strokeLastCanvasPosition_ = event.canvasPosition;
    strokeHasLastCanvasPosition_ = true;

    stats_.recordStroke(timer.nsecsElapsed() / 1'000'000.0, dirty, samples);
    if (changed) {
        document_.setModified(true);
        update();
    } else if (event.kind == InputEventKind::Press) {
        Q_EMIT statusMessage(QStringLiteral("Eraser stroke is outside the active layer"));
    }
}

void OpenGLCanvasWidget::renderFill(const TabletInputEvent &event)
{
    if (!activeLayerWritable()) {
        return;
    }
    QImage *image = document_.activeImage();
    if (!image) {
        Q_EMIT statusMessage(QStringLiteral("No active layer image for fill"));
        return;
    }
    QPoint pixel;
    if (!canvasPixelFromPosition(event.canvasPosition, &pixel, QStringLiteral("Fill"))) {
        return;
    }

    const DocumentState before = document_.snapshot();
    DocumentState after = before;
    if (after.activeLayerIndex < 0 || after.activeLayerIndex >= after.layers.size()) {
        Q_EMIT statusMessage(QStringLiteral("No active layer for fill"));
        return;
    }
    QImage *target = &after.layers[after.activeLayerIndex].image;
    const bool alphaLocked = after.layers[after.activeLayerIndex].alphaLock;
    const QImage alphaBefore = alphaLocked ? before.layers[after.activeLayerIndex].image : QImage();
    FillResult result;
    if (fillSettings_.sampleAllLayers) {
        CanvasDocument sampledDocument;
        sampledDocument.restore(before);
        QImage sampled = sampledDocument.compositedImage(true).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const FillRegion region = FillEngine::matchedRegion(sampled, pixel, fillSettings_);
        result = FillEngine::fillRegion(*target, region, foregroundColor_, fillSettings_);
    } else {
        result = FillEngine::floodFill(*target, pixel, foregroundColor_, fillSettings_);
    }
    if (!result.hasChanges()) {
        if (colorWithFillOpacity(foregroundColor_, fillSettings_.opacity).alpha() == 0) {
            Q_EMIT statusMessage(QStringLiteral("Fill made no visible change because the fill color is transparent"));
        } else {
            Q_EMIT statusMessage(QStringLiteral("Fill made no changes"));
        }
        return;
    }
    if (alphaLocked) {
        restoreAlphaInRect(*target, alphaBefore, result.dirtyRect);
    }
    after.modified = true;
    commandManager_.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Fill"), &document_, before, after));
    Q_EMIT statusMessage(QStringLiteral("Fill changed %1 pixels").arg(result.pixelsChanged));
}

void OpenGLCanvasWidget::pickColor(const TabletInputEvent &event)
{
    const QImage merged = document_.compositedImage();
    const QPoint p = event.canvasPosition.toPoint();
    if (!QRect(QPoint(0, 0), merged.size()).contains(p)) {
        return;
    }
    const QColor color = QColor::fromRgba(merged.pixel(p));
    setForegroundColor(color);
    Q_EMIT colorPicked(color);
    Q_EMIT statusMessage(QStringLiteral("Picked color %1").arg(color.name(QColor::HexArgb)));
}

void OpenGLCanvasWidget::handleSelection(const TabletInputEvent &event)
{
    if (event.kind == InputEventKind::Press && event.isPrimaryDown()) {
        selecting_ = true;
        selectionStart_ = event.canvasPosition;
        selectionBeforeDrag_ = selectionRect_;
        selectionRect_ = normalizedCanvasRect(selectionStart_, event.canvasPosition, document_.size());
        selectionPolygon_ = polygonForRect(QRectF(selectionRect_));
        update();
        return;
    }

    if (selecting_ && (event.kind == InputEventKind::Move || event.kind == InputEventKind::Release)) {
        const QRect dragRect = normalizedCanvasRect(selectionStart_, event.canvasPosition, document_.size());
        selectionRect_ = dragRect;
        selectionPolygon_ = polygonForRect(QRectF(selectionRect_));
        if (event.kind == InputEventKind::Release) {
            selecting_ = false;
            if (dragRect.width() < 2 || dragRect.height() < 2) {
                selectionRect_ = QRect();
                selectionPolygon_.clear();
                Q_EMIT statusMessage(QStringLiteral("Selection cleared"));
            } else {
                if (selectionMode_ == QStringLiteral("Add") && selectionBeforeDrag_.isValid()) {
                    selectionRect_ = selectionBeforeDrag_.united(dragRect);
                } else if (selectionMode_ == QStringLiteral("Intersect") && selectionBeforeDrag_.isValid()) {
                    selectionRect_ = selectionBeforeDrag_.intersected(dragRect);
                } else if (selectionMode_ == QStringLiteral("Subtract") && selectionBeforeDrag_.intersects(dragRect)) {
                    selectionRect_ = subtractAsSingleRect(selectionBeforeDrag_, dragRect);
                }
                if (selectionRect_.isValid() && selectionFeather_ > 0) {
                    selectionRect_ = selectionRect_.adjusted(-selectionFeather_, -selectionFeather_, selectionFeather_, selectionFeather_)
                        .intersected(QRect(QPoint(0, 0), document_.size()));
                }
                selectionPolygon_ = polygonForRect(QRectF(selectionRect_));
                Q_EMIT statusMessage(QStringLiteral("Selection: %1 x %2").arg(selectionRect_.width()).arg(selectionRect_.height()));
                Q_EMIT selectionChanged(selectionRect_);
            }
        }
        update();
    }
}

void OpenGLCanvasWidget::handleTransformTool(const TabletInputEvent &event)
{
    const QRect canvasBounds(QPoint(0, 0), document_.size());

    if (event.kind == InputEventKind::Press && event.isPrimaryDown()) {
        transformInteraction_ = transformHitTest(event.canvasPosition);
        transformStartPoint_ = event.canvasPosition;

        if (transformInteraction_ == TransformHit::Create) {
            selectionStart_ = event.canvasPosition;
            selectionRect_ = normalizedCanvasRect(selectionStart_, event.canvasPosition, document_.size());
            selectionPolygon_ = polygonForRect(QRectF(selectionRect_));
            update();
            Q_EMIT selectionChanged(selectionRect_);
            return;
        }

        if (!activeLayerWritable()) {
            transformInteraction_ = TransformHit::None;
            return;
        }

        const DocumentState before = document_.snapshot();
        if (before.activeLayerIndex < 0 || before.activeLayerIndex >= before.layers.size()) {
            Q_EMIT statusMessage(QStringLiteral("No active layer for transform"));
            transformInteraction_ = TransformHit::None;
            return;
        }

        transformStartSelection_ = selectionRect_.intersected(canvasBounds);
        if (!transformStartSelection_.isValid() || transformStartSelection_.width() < 1 || transformStartSelection_.height() < 1) {
            Q_EMIT statusMessage(QStringLiteral("Create a selection before transforming it"));
            transformInteraction_ = TransformHit::None;
            return;
        }

        transformBefore_ = before;
        transformSourceImage_ = before.layers[before.activeLayerIndex].image.copy(transformStartSelection_);
        transformStartPolygon_ = selectionPolygon_.isEmpty()
            ? polygonForRect(QRectF(transformStartSelection_))
            : selectionPolygon_;
        transformResizeHandleIndex_ = transformInteraction_ == TransformHit::Resize
            ? transformResizeHandleIndexFor(event.canvasPosition)
            : -1;
        const QPointF center = polygonCenter(transformStartPolygon_);
        transformStartAngle_ = std::atan2(event.canvasPosition.y() - center.y(),
                                          event.canvasPosition.x() - center.x());
        return;
    }

    if (transformInteraction_ == TransformHit::None) {
        return;
    }

    if (event.kind != InputEventKind::Move && event.kind != InputEventKind::Release) {
        return;
    }

    if (transformInteraction_ == TransformHit::Create) {
        const QRect dragRect = normalizedCanvasRect(selectionStart_, event.canvasPosition, document_.size());
        selectionRect_ = dragRect;
        selectionPolygon_ = polygonForRect(QRectF(selectionRect_));
        if (event.kind == InputEventKind::Release) {
            transformInteraction_ = TransformHit::None;
            if (dragRect.width() < 2 || dragRect.height() < 2) {
                selectionRect_ = QRect();
                selectionPolygon_.clear();
                Q_EMIT statusMessage(QStringLiteral("Selection cleared"));
            } else {
                Q_EMIT statusMessage(QStringLiteral("Transform selection: %1 x %2")
                    .arg(selectionRect_.width())
                    .arg(selectionRect_.height()));
            }
        }
        update();
        Q_EMIT selectionChanged(selectionRect_);
        return;
    }

    updateTransformPreview(event.canvasPosition);

    if (event.kind == InputEventKind::Release) {
        const DocumentState after = document_.snapshot();
        const QRect finalSelection = selectionRect_;
        const QPolygonF finalPolygon = selectionPolygon_;
        const QRect beforeSelection = transformStartSelection_;
        const QPolygonF beforePolygon = transformStartPolygon_.isEmpty()
            ? polygonForRect(QRectF(beforeSelection))
            : transformStartPolygon_;
        bool changed = false;
        if (transformBefore_.activeLayerIndex >= 0
            && transformBefore_.activeLayerIndex < transformBefore_.layers.size()
            && after.activeLayerIndex >= 0
            && after.activeLayerIndex < after.layers.size()) {
            changed = transformBefore_.layers[transformBefore_.activeLayerIndex].image
                != after.layers[after.activeLayerIndex].image;
        }

        document_.restore(transformBefore_);
        if (changed) {
            commandManager_.execute(std::make_unique<FunctionalCommand>(
                QStringLiteral("Transform Selection"),
                [this, before = transformBefore_, beforeSelection, beforePolygon] {
                    document_.restore(before);
                    selectionRect_ = beforeSelection;
                    selectionPolygon_ = beforePolygon;
                    update();
                    Q_EMIT selectionChanged(selectionRect_);
                },
                [this, after, finalSelection, finalPolygon] {
                    document_.restore(after);
                    selectionRect_ = finalSelection;
                    selectionPolygon_ = finalPolygon;
                    update();
                    Q_EMIT selectionChanged(selectionRect_);
                }));
            selectionRect_ = finalSelection;
            selectionPolygon_ = finalPolygon;
            Q_EMIT statusMessage(QStringLiteral("Selection transformed: %1 x %2")
                .arg(selectionRect_.width())
                .arg(selectionRect_.height()));
        } else {
            selectionRect_ = transformStartSelection_;
            selectionPolygon_ = beforePolygon;
            update();
            Q_EMIT statusMessage(QStringLiteral("Selection transform made no changes"));
        }

        transformInteraction_ = TransformHit::None;
        transformSourceImage_ = QImage();
        transformStartPolygon_.clear();
        update();
        Q_EMIT selectionChanged(selectionRect_);
    }
}

void OpenGLCanvasWidget::handleShape(const TabletInputEvent &event)
{
    if (event.kind == InputEventKind::Press && event.isPrimaryDown()) {
        if (!activeLayerWritable()) {
            return;
        }
        shaping_ = true;
        shapeStart_ = event.canvasPosition;
        shapeBefore_ = document_.snapshot();
        shapePreviewRect_ = normalizedCanvasRect(shapeStart_, event.canvasPosition, document_.size());
        update();
        return;
    }

    if (!shaping_) {
        return;
    }

    if (event.kind == InputEventKind::Move || event.kind == InputEventKind::Release) {
        shapePreviewRect_ = normalizedCanvasRect(shapeStart_, event.canvasPosition, document_.size());
        if (event.kind == InputEventKind::Release) {
            shaping_ = false;
            if (shapePreviewRect_.width() >= 2 && shapePreviewRect_.height() >= 2) {
                QImage *image = document_.activeImage();
                if (image) {
                    {
                        QPainter painter(image);
                        painter.setRenderHint(QPainter::Antialiasing, true);
                        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
                        painter.setPen(QPen(foregroundColor_, shapeStrokeWidth_));
                        painter.setBrush(shapeFillEnabled_ ? QBrush(backgroundColor_) : QBrush(Qt::NoBrush));
                        painter.drawRect(shapePreviewRect_.adjusted(1, 1, -1, -1));
                    }
                    document_.setModified(true);
                    commandManager_.execute(std::make_unique<DocumentSnapshotCommand>(
                        QStringLiteral("Shape"), &document_, shapeBefore_, document_.snapshot()));
                    Q_EMIT statusMessage(QStringLiteral("Shape drawn"));
                }
            }
            shapePreviewRect_ = QRect();
        }
        update();
    }
}

void OpenGLCanvasWidget::handleMoveTool(const TabletInputEvent &event)
{
    if (event.kind == InputEventKind::Press && event.isPrimaryDown()) {
        if (!activeLayerWritable()) {
            return;
        }
        QImage *image = document_.activeImage();
        if (!image) {
            return;
        }
        movingLayer_ = true;
        layerMoveChanged_ = false;
        layerMoveStart_ = event.canvasPosition;
        layerMoveOriginal_ = image->copy();
        layerMoveBefore_ = document_.snapshot();
        return;
    }

    if (!movingLayer_) {
        return;
    }

    if (event.kind == InputEventKind::Move || event.kind == InputEventKind::Release) {
        QImage *image = document_.activeImage();
        if (!image) {
            movingLayer_ = false;
            return;
        }
        const QPoint delta = (event.canvasPosition - layerMoveStart_).toPoint();
        QImage moved(layerMoveOriginal_.size(), QImage::Format_ARGB32_Premultiplied);
        moved.fill(Qt::transparent);
        QPainter painter(&moved);
        painter.drawImage(delta, layerMoveOriginal_);
        *image = moved;
        layerMoveChanged_ = layerMoveChanged_ || !delta.isNull();
        document_.setModified(true);
        notifyChanged();

        if (event.kind == InputEventKind::Release) {
            movingLayer_ = false;
            if (layerMoveChanged_) {
                commandManager_.execute(std::make_unique<DocumentSnapshotCommand>(
                    QStringLiteral("Move Layer Pixels"), &document_, layerMoveBefore_, document_.snapshot()));
            }
            layerMoveOriginal_ = QImage();
        }
    }
}

void OpenGLCanvasWidget::insertText(const TabletInputEvent &event)
{
    if (!activeLayerWritable()) {
        return;
    }

    const QPointF position = hasSelection() ? QPointF(selectionRect_.topLeft()) : event.canvasPosition;
    if (!QRectF(QPointF(0, 0), QSizeF(document_.size())).contains(position)) {
        Q_EMIT statusMessage(QStringLiteral("Text position is outside the canvas"));
        return;
    }

    TextSettings dialogSettings = textSettings_;
    dialogSettings.color = foregroundColor_;
    TextSettingsDialog dialog(this);
    dialog.setSettings(dialogSettings);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QFont font = this->font();
    TextSettings settings = dialog.settings();
    if (!settings.font.family().isEmpty()) {
        font.setFamily(settings.font.family());
    }
    font.setPointSize(settings.font.pointSize() > 0 ? settings.font.pointSize() : 32);
    settings.font = font;
    if (hasSelection()) {
        settings.boxSize = selectionRect_.size();
    }
    textSettings_ = settings;
    foregroundColor_ = settings.color;
    brushSettings_.color = foregroundColor_;
    Q_EMIT colorPicked(foregroundColor_);

    insertTextAt(position, dialog.text(), settings);
}

void OpenGLCanvasWidget::insertTextAt(const QPointF &position, const QString &text, const TextSettings &settings)
{
    const DocumentState before = document_.snapshot();
    DocumentState after = before;
    if (after.activeLayerIndex < 0 || after.activeLayerIndex >= after.layers.size()) {
        Q_EMIT statusMessage(QStringLiteral("No active layer for text"));
        return;
    }

    TextSettings effectiveSettings = settings;
    if (effectiveSettings.color.alpha() == 0) {
        Q_EMIT statusMessage(QStringLiteral("Text color is fully transparent"));
        return;
    }

    QImage &target = after.layers[after.activeLayerIndex].image;
    const TextRenderResult result = TextEngine::drawTextBox(target, position, text, effectiveSettings);
    if (!result.changed) {
        Q_EMIT statusMessage(QStringLiteral("Text was not inserted"));
        return;
    }
    after.modified = true;
    commandManager_.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Text"), &document_, before, after));
    clearSelection();
    Q_EMIT statusMessage(QStringLiteral("Inserted text at %1 pt").arg(effectiveSettings.font.pointSize()));
}

bool OpenGLCanvasWidget::activeLayerWritable()
{
    const Layer *layer = document_.activeLayer();
    if (!layer) {
        Q_EMIT statusMessage(QStringLiteral("No active layer"));
        return false;
    }
    if (layer->locked) {
        Q_EMIT statusMessage(QStringLiteral("Layer is locked"));
        return false;
    }
    return true;
}

bool OpenGLCanvasWidget::canvasPixelFromPosition(const QPointF &position, QPoint *pixel, const QString &operationName)
{
    const QRect bounds(QPoint(0, 0), document_.size());
    const QPoint candidate(static_cast<int>(std::floor(position.x())),
                           static_cast<int>(std::floor(position.y())));
    if (!bounds.contains(candidate)) {
        Q_EMIT statusMessage(QStringLiteral("%1 position is outside the canvas").arg(operationName));
        return false;
    }
    if (pixel) {
        *pixel = candidate;
    }
    return true;
}

bool OpenGLCanvasWidget::shouldSuppressSynthesizedMouseEvent(const QMouseEvent &event) const
{
    const bool primaryInvolved = event.buttons().testFlag(Qt::LeftButton)
        || event.button() == Qt::LeftButton;
    if (!primaryInvolved) {
        return false;
    }
    const bool release = event.type() == QEvent::MouseButtonRelease;
    if (drawing_ && strokeDevice_ != InputDeviceKind::Mouse && strokeDevice_ != InputDeviceKind::Unknown) {
        return !release;
    }
    if (!inputClock_.isValid()) {
        return false;
    }

    const qint64 elapsedSinceTablet = inputClock_.elapsed() - lastTabletEventMs_;
    if (elapsedSinceTablet < 0) {
        return false;
    }
    if (elapsedSinceTablet <= 80) {
        return true;
    }
    const double distance = std::hypot(event.position().x() - lastTabletEventWidgetPosition_.x(),
                                      event.position().y() - lastTabletEventWidgetPosition_.y());
    return elapsedSinceTablet <= 500 && distance <= 24.0;
}

bool OpenGLCanvasWidget::shouldIgnoreDuplicateStrokePress(const TabletInputEvent &event, ToolType active) const
{
    if (!drawing_ || event.kind != InputEventKind::Press || !event.isPrimaryDown()) {
        return false;
    }
    if (event.device == InputDeviceKind::Mouse
        && strokeDevice_ != InputDeviceKind::Mouse
        && strokeDevice_ != InputDeviceKind::Unknown) {
        return true;
    }
    if (active != strokeTool_ || !strokeHasLastCanvasPosition_) {
        return false;
    }

    const QPointF delta = event.canvasPosition - strokeLastCanvasPosition_;
    const double duplicateRadius = std::max(1.0, 3.0 / std::max(zoom_, 0.05));
    return std::hypot(delta.x(), delta.y()) <= duplicateRadius;
}

void OpenGLCanvasWidget::resetInteractionState()
{
    brushEngine_.endStroke();
    eraserEngine_.endStroke();
    drawing_ = false;
    strokeChanged_ = false;
    strokeDevice_ = InputDeviceKind::Unknown;
    strokeHasLastCanvasPosition_ = false;
    selecting_ = false;
    transformInteraction_ = TransformHit::None;
    transformSourceImage_ = QImage();
    transformStartPolygon_.clear();
    transformResizeHandleIndex_ = -1;
    shaping_ = false;
    shapePreviewRect_ = QRect();
    movingLayer_ = false;
    layerMoveChanged_ = false;
    layerMoveOriginal_ = QImage();
}

OpenGLCanvasWidget::TransformHit OpenGLCanvasWidget::transformHitTest(const QPointF &canvasPosition) const
{
    if (!hasSelection()) {
        return TransformHit::Create;
    }

    const QPolygonF selection = selectionPolygon_.isEmpty()
        ? polygonForRect(QRectF(selectionRect_))
        : selectionPolygon_;
    if (selection.size() < 4) {
        return TransformHit::Create;
    }

    const double handleRadius = std::max(10.0, 18.0 / std::max(zoom_, 0.05));
    const auto nearPoint = [handleRadius, &canvasPosition](const QPointF &point) {
        return std::hypot(canvasPosition.x() - point.x(), canvasPosition.y() - point.y()) <= handleRadius;
    };

    const QPointF rotatePoint = rotationHandleForPolygon(selection, handleRadius);
    if (nearPoint(rotatePoint)) {
        return TransformHit::Rotate;
    }

    const QVector<QPointF> handles = transformHandlesForPolygon(selection);
    for (const QPointF &handle : handles) {
        if (nearPoint(handle)) {
            return TransformHit::Resize;
        }
    }

    QPainterPath selectionPath;
    selectionPath.addPolygon(selection);
    if (selectionPath.contains(canvasPosition)) {
        QTransform toLocal;
        const QRectF bounds(selectionRect_);
        if (!bounds.isEmpty()) {
            const QPolygonF local = polygonForRect(bounds);
            if (QTransform::quadToQuad(selection, local, toLocal)) {
                const QPointF localPoint = toLocal.map(canvasPosition);
                const double localRadius = std::max(8.0, handleRadius);
                const bool nearEdge = localPoint.x() <= bounds.left() + localRadius
                    || localPoint.x() >= bounds.right() - localRadius
                    || localPoint.y() <= bounds.top() + localRadius
                    || localPoint.y() >= bounds.bottom() - localRadius;
                if (bounds.width() > localRadius * 3.0 && bounds.height() > localRadius * 3.0 && nearEdge) {
                    return TransformHit::Resize;
                }
            }
        }
        return TransformHit::Move;
    }

    return TransformHit::Create;
}

int OpenGLCanvasWidget::transformResizeHandleIndexFor(const QPointF &canvasPosition) const
{
    const QPolygonF selection = selectionPolygon_.isEmpty()
        ? polygonForRect(QRectF(selectionRect_))
        : selectionPolygon_;
    const QVector<QPointF> handles = transformHandlesForPolygon(selection);
    if (handles.size() != 8) {
        return -1;
    }

    int closest = 0;
    double closestDistance = std::numeric_limits<double>::max();
    for (int i = 0; i < handles.size(); ++i) {
        const double distance = std::hypot(canvasPosition.x() - handles[i].x(),
                                           canvasPosition.y() - handles[i].y());
        if (distance < closestDistance) {
            closestDistance = distance;
            closest = i;
        }
    }

    return closest;
}

QPolygonF OpenGLCanvasWidget::transformResizeTargetPolygon(const QPointF &canvasPosition) const
{
    if (transformStartPolygon_.size() < 4 || transformResizeHandleIndex_ < 0) {
        return {};
    }

    const QRectF baseRect(transformStartSelection_);
    if (!baseRect.isValid() || baseRect.width() < 1.0 || baseRect.height() < 1.0) {
        return {};
    }

    QTransform canvasToLocal;
    if (!QTransform::quadToQuad(transformStartPolygon_, polygonForRect(baseRect), canvasToLocal)) {
        return {};
    }

    const QPointF localMouse = canvasToLocal.map(canvasPosition);
    QRectF resized = baseRect;
    constexpr double minSize = 1.0;
    switch (transformResizeHandleIndex_) {
    case 0:
        resized.setLeft(std::min(localMouse.x(), baseRect.right() - minSize));
        resized.setTop(std::min(localMouse.y(), baseRect.bottom() - minSize));
        break;
    case 1:
        resized.setTop(std::min(localMouse.y(), baseRect.bottom() - minSize));
        break;
    case 2:
        resized.setRight(std::max(localMouse.x(), baseRect.left() + minSize));
        resized.setTop(std::min(localMouse.y(), baseRect.bottom() - minSize));
        break;
    case 3:
        resized.setRight(std::max(localMouse.x(), baseRect.left() + minSize));
        break;
    case 4:
        resized.setRight(std::max(localMouse.x(), baseRect.left() + minSize));
        resized.setBottom(std::max(localMouse.y(), baseRect.top() + minSize));
        break;
    case 5:
        resized.setBottom(std::max(localMouse.y(), baseRect.top() + minSize));
        break;
    case 6:
        resized.setLeft(std::min(localMouse.x(), baseRect.right() - minSize));
        resized.setBottom(std::max(localMouse.y(), baseRect.top() + minSize));
        break;
    case 7:
        resized.setLeft(std::min(localMouse.x(), baseRect.right() - minSize));
        break;
    default:
        return {};
    }

    if (resized.width() < 1.0 || resized.height() < 1.0) {
        return {};
    }

    QTransform localToCanvas;
    if (!QTransform::quadToQuad(polygonForRect(baseRect), transformStartPolygon_, localToCanvas)) {
        return {};
    }
    return localToCanvas.map(polygonForRect(resized));
}

bool OpenGLCanvasWidget::buildSelectionTransformState(const DocumentState &sourceState,
                                                      const QRect &sourceRect,
                                                      const QImage &sourceImage,
                                                      const QPointF &targetCenter,
                                                      const QSize &targetSize,
                                                      double rotationDegrees,
                                                      DocumentState *resultState,
                                                      QRect *resultSelection,
                                                      QPolygonF *resultPolygon) const
{
    const QSize safeTargetSize(std::max(1, targetSize.width()), std::max(1, targetSize.height()));
    QPolygonF targetPolygon = polygonForRect(QRectF(QPointF(targetCenter.x() - safeTargetSize.width() * 0.5,
                                                            targetCenter.y() - safeTargetSize.height() * 0.5),
                                                    QSizeF(safeTargetSize)));
    if (std::abs(rotationDegrees) > 0.001) {
        QTransform transform;
        transform.translate(targetCenter.x(), targetCenter.y());
        transform.rotate(rotationDegrees);
        transform.translate(-targetCenter.x(), -targetCenter.y());
        targetPolygon = transform.map(targetPolygon);
    }

    const QPolygonF sourcePolygon = selectionPolygon_.isEmpty()
        ? polygonForRect(QRectF(sourceRect))
        : selectionPolygon_;
    return buildSelectionTransformStateForPolygon(sourceState,
                                                  sourceRect,
                                                  sourceImage,
                                                  sourcePolygon,
                                                  targetPolygon,
                                                  resultState,
                                                  resultSelection,
                                                  resultPolygon);
}

bool OpenGLCanvasWidget::buildSelectionTransformStateForPolygon(const DocumentState &sourceState,
                                                                const QRect &sourceRect,
                                                                const QImage &sourceImage,
                                                                const QPolygonF &sourcePolygon,
                                                                const QPolygonF &targetPolygon,
                                                                DocumentState *resultState,
                                                                QRect *resultSelection,
                                                                QPolygonF *resultPolygon) const
{
    if (!resultState || sourceImage.isNull() || !sourceRect.isValid() || sourcePolygon.size() < 4 || targetPolygon.size() < 4) {
        return false;
    }
    if (sourceState.activeLayerIndex < 0 || sourceState.activeLayerIndex >= sourceState.layers.size()) {
        return false;
    }

    *resultState = sourceState;
    QImage &layerImage = resultState->layers[resultState->activeLayerIndex].image;
    if (layerImage.isNull()) {
        return false;
    }

    const QRect canvasBounds(QPoint(0, 0), resultState->canvasSize);
    const QRect clearedSource = sourceRect.intersected(canvasBounds);
    const QRect visibleDestination = targetPolygon.boundingRect().toAlignedRect().intersected(canvasBounds);
    if (!clearedSource.isValid() || !visibleDestination.isValid()) {
        return false;
    }

    QPolygonF sourceLocal;
    sourceLocal.reserve(sourcePolygon.size());
    const QPointF sourceOffset(sourceRect.topLeft());
    for (const QPointF &point : sourcePolygon) {
        sourceLocal << (point - sourceOffset);
    }

    QTransform imageTransform;
    if (!QTransform::quadToQuad(sourceLocal, targetPolygon, imageTransform)) {
        return false;
    }

    QPainter painter(&layerImage);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    QPainterPath clearPath;
    clearPath.addPolygon(sourcePolygon);
    clearPath.closeSubpath();
    painter.setClipPath(clearPath);
    painter.fillRect(clearedSource, Qt::transparent);
    painter.setClipping(false);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath clipPath;
    clipPath.addPolygon(targetPolygon);
    painter.setClipPath(clipPath);
    painter.setTransform(imageTransform, true);
    painter.drawImage(QPointF(0, 0), sourceImage.convertToFormat(QImage::Format_ARGB32_Premultiplied));
    painter.end();

    resultState->modified = true;
    if (resultSelection) {
        *resultSelection = visibleDestination;
    }
    if (resultPolygon) {
        *resultPolygon = targetPolygon;
    }
    return true;
}

void OpenGLCanvasWidget::updateTransformPreview(const QPointF &canvasPosition)
{
    if (transformInteraction_ == TransformHit::None || transformSourceImage_.isNull()) {
        return;
    }

    QPointF targetCenter = QRectF(transformStartSelection_).center();
    QSize targetSize = transformStartSelection_.size();
    double rotationDegrees = 0.0;
    QPolygonF targetPolygon;

    if (transformInteraction_ == TransformHit::Move) {
        targetPolygon = translatedPolygon(transformStartPolygon_, canvasPosition - transformStartPoint_);
    } else if (transformInteraction_ == TransformHit::Resize) {
        targetPolygon = transformResizeTargetPolygon(canvasPosition);
        if (targetPolygon.size() < 4) {
            return;
        }
    } else if (transformInteraction_ == TransformHit::Rotate) {
        const QPointF center = transformStartPolygon_.isEmpty()
            ? QRectF(transformStartSelection_).center()
            : polygonCenter(transformStartPolygon_);
        const double currentAngle = std::atan2(canvasPosition.y() - center.y(),
                                               canvasPosition.x() - center.x());
        rotationDegrees = radiansToDegrees(currentAngle - transformStartAngle_);
        QTransform polygonTransform;
        polygonTransform.translate(center.x(), center.y());
        polygonTransform.rotate(rotationDegrees);
        polygonTransform.translate(-center.x(), -center.y());
        targetPolygon = polygonTransform.map(transformStartPolygon_);
    }

    DocumentState preview;
    QRect previewSelection;
    QPolygonF previewPolygon;
    if (targetPolygon.size() >= 4) {
        if (!buildSelectionTransformStateForPolygon(transformBefore_,
                                                    transformStartSelection_,
                                                    transformSourceImage_,
                                                    transformStartPolygon_,
                                                    targetPolygon,
                                                    &preview,
                                                    &previewSelection,
                                                    &previewPolygon)) {
            return;
        }
    } else if (!buildSelectionTransformState(transformBefore_,
                                            transformStartSelection_,
                                            transformSourceImage_,
                                            targetCenter,
                                            targetSize,
                                            rotationDegrees,
                                            &preview,
                                            &previewSelection,
                                            &previewPolygon)) {
        return;
    }

    document_.restore(preview);
    selectionPolygon_ = previewPolygon;
    selectionRect_ = previewSelection;
    update();
    Q_EMIT selectionChanged(selectionRect_);
}

void OpenGLCanvasWidget::finishStroke()
{
    if (!drawing_) {
        return;
    }
    brushEngine_.endStroke();
    eraserEngine_.endStroke();
    drawing_ = false;

    if (strokeChanged_) {
        const QString name = strokeTool_ == ToolType::Eraser
            ? QStringLiteral("Erase Stroke")
            : QStringLiteral("Brush Stroke");
        commandManager_.execute(std::make_unique<DocumentSnapshotCommand>(
            name, &document_, strokeBefore_, document_.snapshot()));
    }
    strokeChanged_ = false;
    strokeDevice_ = InputDeviceKind::Unknown;
    strokeHasLastCanvasPosition_ = false;
}

void OpenGLCanvasWidget::zoomAt(const QPointF &widgetPosition, double scaleFactor)
{
    const QPointF before = widgetToCanvas(widgetPosition);
    zoom_ = std::clamp(zoom_ * scaleFactor, 0.05, 16.0);
    pan_ = widgetPosition - before * zoom_;
    update();
    Q_EMIT zoomChanged(zoom_);
}

void OpenGLCanvasWidget::notifyChanged()
{
    update();
    Q_EMIT documentChanged();
}

#pragma once

#include "brush/BrushEngine.h"
#include "brush/EraserEngine.h"
#include "brush/FillEngine.h"
#include "brush/TextEngine.h"
#include "commands/CommandManager.h"
#include "core/PerformanceStats.h"
#include "document/LayerManager.h"
#include "render/CanvasDocument.h"
#include "tools/ToolManager.h"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPolygonF>
#include <QRect>
#include <QString>
#include <QTransform>

class OpenGLCanvasWidget final : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit OpenGLCanvasWidget(QWidget *parent = nullptr);

    void setToolManager(ToolManager *toolManager);

    void setBrushColor(const QColor &color);
    void setForegroundColor(const QColor &color);
    void setBackgroundColor(const QColor &color);
    void setBrushRadius(double radius);
    void setBrushOpacity(double opacity);
    void setPressureControlsRadius(bool enabled);
    void setPressureControlsOpacity(bool enabled);
    void setBrushSettings(const BrushSettings &settings);
    void setEraserSettings(const EraserSettings &settings);
    void setFillSettings(const FillSettings &settings);
    void setTextSettings(const TextSettings &settings);
    void setSelectionSettings(const QString &mode, int feather, bool antiAlias);
    void setShapeSettings(int strokeWidth, bool fillEnabled);

    [[nodiscard]] QColor brushColor() const;
    [[nodiscard]] QColor foregroundColor() const;
    [[nodiscard]] QColor backgroundColor() const;
    [[nodiscard]] BrushSettings brushSettings() const;
    [[nodiscard]] EraserSettings eraserSettings() const;
    [[nodiscard]] FillSettings fillSettings() const;
    [[nodiscard]] TextSettings textSettings() const;
    [[nodiscard]] StatsSnapshot statsSnapshot() const;
    [[nodiscard]] const CanvasDocument &document() const;
    CanvasDocument &document();
    [[nodiscard]] CommandManager *commandManager();
    [[nodiscard]] LayerManager *layerManager();
    [[nodiscard]] bool hasDocument() const;
    [[nodiscard]] double zoom() const;
    [[nodiscard]] bool hasSelection() const;
    [[nodiscard]] QRect selectionRect() const;

    void newDocument(const NewDocumentSettings &settings);
    void newDocument(const QSize &size);
    bool openImage(const QString &filePath);
    bool saveImage(const QString &filePath) const;
    void restoreDocument(const DocumentState &state);
    void notifyDocumentLoaded(const QString &filePath = QString());

public Q_SLOTS:
    void zoomIn();
    void zoomOut();
    void fitCanvas();
    void resetView();
    void rotateViewLeft();
    void rotateViewRight();
    void setSelectionRect(const QRect &selection);
    void transformSelection(double rotationDegrees, int targetWidth, int targetHeight, int scalePercent);
    void clearSelection();

Q_SIGNALS:
    void documentChanged();
    void cursorPositionChanged(const QPointF &canvasPosition);
    void zoomChanged(double zoom);
    void statusMessage(const QString &message);
    void colorPicked(const QColor &color);
    void selectionChanged(const QRect &selection);

protected:
    void paintGL() override;
    void resizeGL(int width, int height) override;
    void tabletEvent(QTabletEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    enum class TransformHit {
        None,
        Create,
        Move,
        Resize,
        Rotate
    };

    QPointF canvasToWidget(const QPointF &canvasPosition) const;
    QPointF widgetToCanvas(const QPointF &widgetPosition) const;
    QTransform canvasTransform() const;
    void centerCanvas();
    void drawCheckerboard(QPainter &painter, const QRectF &canvasRect);
    void handleInput(const TabletInputEvent &event);
    void renderBrushStroke(const TabletInputEvent &event);
    void renderEraserStroke(const TabletInputEvent &event);
    void renderFill(const TabletInputEvent &event);
    void pickColor(const TabletInputEvent &event);
    void handleSelection(const TabletInputEvent &event);
    void handleTransformTool(const TabletInputEvent &event);
    void handleShape(const TabletInputEvent &event);
    void handleMoveTool(const TabletInputEvent &event);
    void insertText(const TabletInputEvent &event);
    void insertTextAt(const QPointF &position, const QString &text, const TextSettings &settings);
    bool activeLayerWritable();
    bool canvasPixelFromPosition(const QPointF &position, QPoint *pixel, const QString &operationName);
    bool shouldSuppressSynthesizedMouseEvent(const QMouseEvent &event) const;
    bool shouldIgnoreDuplicateStrokePress(const TabletInputEvent &event, ToolType active) const;
    void resetInteractionState();
    TransformHit transformHitTest(const QPointF &canvasPosition) const;
    int transformResizeHandleIndexFor(const QPointF &canvasPosition) const;
    QPolygonF transformResizeTargetPolygon(const QPointF &canvasPosition) const;
    bool buildSelectionTransformState(const DocumentState &sourceState,
                                      const QRect &sourceRect,
                                      const QImage &sourceImage,
                                      const QPointF &targetCenter,
                                      const QSize &targetSize,
                                      double rotationDegrees,
                                      DocumentState *resultState,
                                      QRect *resultSelection,
                                      QPolygonF *resultPolygon) const;
    bool buildSelectionTransformStateForPolygon(const DocumentState &sourceState,
                                                const QRect &sourceRect,
                                                const QImage &sourceImage,
                                                const QPolygonF &sourcePolygon,
                                                const QPolygonF &targetPolygon,
                                                DocumentState *resultState,
                                                QRect *resultSelection,
                                                QPolygonF *resultPolygon) const;
    void updateTransformPreview(const QPointF &canvasPosition);
    void finishStroke();
    void zoomAt(const QPointF &widgetPosition, double scaleFactor);
    void notifyChanged();

    CanvasDocument document_;
    CommandManager commandManager_;
    LayerManager *layerManager_ = nullptr;
    ToolManager *toolManager_ = nullptr;
    BrushEngine brushEngine_;
    EraserEngine eraserEngine_;
    BrushSettings brushSettings_;
    EraserSettings eraserSettings_;
    FillSettings fillSettings_;
    TextSettings textSettings_;
    PerformanceStats stats_;
    QColor foregroundColor_ = QColor(28, 29, 31);
    QColor backgroundColor_ = Qt::white;
    QPointF pan_;
    QPointF lastPanPosition_;
    QElapsedTimer inputClock_;
    qint64 lastTabletEventMs_ = -1000;
    QPointF lastTabletEventWidgetPosition_;
    qint64 lastTabletTextPressMs_ = -1000;
    QPointF lastTabletTextPressWidget_;
    bool panning_ = false;
    bool drawing_ = false;
    bool strokeChanged_ = false;
    ToolType strokeTool_ = ToolType::Brush;
    InputDeviceKind strokeDevice_ = InputDeviceKind::Unknown;
    QPointF strokeLastCanvasPosition_;
    bool strokeHasLastCanvasPosition_ = false;
    DocumentState strokeBefore_;
    double zoom_ = 1.0;
    double rotationDegrees_ = 0.0;

    bool selecting_ = false;
    QPointF selectionStart_;
    QRect selectionBeforeDrag_;
    QRect selectionRect_;
    QPolygonF selectionPolygon_;
    QString selectionMode_ = QStringLiteral("Replace");
    int selectionFeather_ = 0;
    bool selectionAntiAlias_ = true;

    TransformHit transformInteraction_ = TransformHit::None;
    QPointF transformStartPoint_;
    int transformResizeHandleIndex_ = -1;
    QRect transformStartSelection_;
    QPolygonF transformStartPolygon_;
    DocumentState transformBefore_;
    QImage transformSourceImage_;
    double transformStartAngle_ = 0.0;

    bool shaping_ = false;
    QPointF shapeStart_;
    QRect shapePreviewRect_;
    DocumentState shapeBefore_;
    int shapeStrokeWidth_ = 2;
    bool shapeFillEnabled_ = false;

    bool movingLayer_ = false;
    bool layerMoveChanged_ = false;
    QPointF layerMoveStart_;
    QImage layerMoveOriginal_;
    DocumentState layerMoveBefore_;
};

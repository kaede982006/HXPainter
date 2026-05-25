#pragma once

#include "brush/BrushSettings.h"
#include "brush/EraserEngine.h"
#include "brush/FillEngine.h"
#include "brush/TextEngine.h"
#include "tools/Tool.h"

#include <QStackedWidget>
#include <QWidget>
#include <QRect>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;

class ToolOptionsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ToolOptionsPanel(QWidget *parent = nullptr);

    void setBrushSettings(const BrushSettings &settings);
    void setEraserSettings(const EraserSettings &settings);
    void setFillSettings(const FillSettings &settings);
    void setTextSettings(const TextSettings &settings);

public Q_SLOTS:
    void setActiveTool(ToolType type);
    void setSelectionBounds(const QRect &selection);

Q_SIGNALS:
    void brushSettingsChanged(const BrushSettings &settings);
    void eraserSettingsChanged(const EraserSettings &settings);
    void fillSettingsChanged(const FillSettings &settings);
    void textSettingsChanged(const TextSettings &settings);
    void selectionSettingsChanged(const QString &mode, int feather, bool antiAlias);
    void selectionTransformRequested(double rotationDegrees, int targetWidth, int targetHeight, int scalePercent);
    void shapeSettingsChanged(int strokeWidth, bool fillEnabled);

private:
    QWidget *buildBrushPage();
    QWidget *buildEraserPage();
    QWidget *buildFillPage();
    QWidget *buildSelectionPage();
    QWidget *buildTransformPage();
    QWidget *buildShapePage();
    QWidget *buildTextPage();
    QWidget *buildPlaceholderPage(const QString &message);

    void emitBrush();
    void emitEraser();
    void emitFill();
    void emitText();
    void emitSelection();
    void emitSelectionTransform();
    void emitShape();

    QStackedWidget *stack_ = nullptr;
    BrushSettings brush_;
    EraserSettings eraser_;
    FillSettings fill_;
    TextSettings text_;

    QSpinBox *brushSize_ = nullptr;
    QSpinBox *brushOpacity_ = nullptr;
    QSpinBox *brushFlow_ = nullptr;
    QSpinBox *brushHardness_ = nullptr;
    QDoubleSpinBox *brushSpacing_ = nullptr;
    QCheckBox *brushPressureSize_ = nullptr;
    QCheckBox *brushPressureOpacity_ = nullptr;

    QSpinBox *eraserSize_ = nullptr;
    QSpinBox *eraserOpacity_ = nullptr;
    QCheckBox *eraserAlphaOnly_ = nullptr;
    QCheckBox *eraserCurrentLayer_ = nullptr;

    QSpinBox *fillTolerance_ = nullptr;
    QCheckBox *fillContiguous_ = nullptr;
    QCheckBox *fillSampleAll_ = nullptr;
    QSpinBox *fillOpacity_ = nullptr;

    QSpinBox *textPointSize_ = nullptr;
    QSpinBox *textBoxWidth_ = nullptr;
    QSpinBox *textBoxHeight_ = nullptr;

    QComboBox *selectionMode_ = nullptr;
    QSpinBox *selectionFeather_ = nullptr;
    QCheckBox *selectionAntialias_ = nullptr;
    QDoubleSpinBox *selectionRotation_ = nullptr;
    QSpinBox *selectionWidth_ = nullptr;
    QSpinBox *selectionHeight_ = nullptr;
    QSpinBox *selectionScale_ = nullptr;
    QPushButton *selectionApplyTransform_ = nullptr;

    QSpinBox *shapeStrokeWidth_ = nullptr;
    QCheckBox *shapeFillEnabled_ = nullptr;
};

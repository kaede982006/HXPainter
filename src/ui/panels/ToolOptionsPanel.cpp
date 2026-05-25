#include "ToolOptionsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

ToolOptionsPanel::ToolOptionsPanel(QWidget *parent)
    : QWidget(parent)
{
    stack_ = new QStackedWidget(this);
    stack_->addWidget(buildBrushPage());
    stack_->addWidget(buildEraserPage());
    stack_->addWidget(buildFillPage());
    stack_->addWidget(buildPlaceholderPage(QStringLiteral("Color picker uses the canvas click position.")));
    stack_->addWidget(buildPlaceholderPage(QStringLiteral("Drag the canvas to move the active layer pixels.")));
    stack_->addWidget(buildSelectionPage());
    stack_->addWidget(buildTransformPage());
    stack_->addWidget(buildShapePage());
    stack_->addWidget(buildTextPage());
    stack_->addWidget(buildPlaceholderPage(QStringLiteral("Pan the canvas with the middle or right mouse button.")));
    stack_->addWidget(buildPlaceholderPage(QStringLiteral("Use the View menu or top toolbar to change zoom.")));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(stack_);
}

void ToolOptionsPanel::setBrushSettings(const BrushSettings &settings)
{
    brush_ = settings;
    if (brushSize_) {
        QSignalBlocker sizeBlocker(brushSize_);
        QSignalBlocker opacityBlocker(brushOpacity_);
        QSignalBlocker flowBlocker(brushFlow_);
        QSignalBlocker hardnessBlocker(brushHardness_);
        QSignalBlocker spacingBlocker(brushSpacing_);
        QSignalBlocker pressureSizeBlocker(brushPressureSize_);
        QSignalBlocker pressureOpacityBlocker(brushPressureOpacity_);
        brushSize_->setValue(static_cast<int>(brush_.radius));
        brushOpacity_->setValue(static_cast<int>(brush_.opacity * 100.0));
        brushFlow_->setValue(static_cast<int>(brush_.flow * 100.0));
        brushHardness_->setValue(static_cast<int>(brush_.hardness * 100.0));
        brushSpacing_->setValue(brush_.spacing);
        brushPressureSize_->setChecked(brush_.pressureControlsRadius);
        brushPressureOpacity_->setChecked(brush_.pressureControlsOpacity);
    }
}

void ToolOptionsPanel::setEraserSettings(const EraserSettings &settings)
{
    eraser_ = settings;
    eraser_.eraseCurrentLayerOnly = true;
    if (eraserSize_) {
        QSignalBlocker sizeBlocker(eraserSize_);
        QSignalBlocker opacityBlocker(eraserOpacity_);
        QSignalBlocker alphaOnlyBlocker(eraserAlphaOnly_);
        QSignalBlocker currentLayerBlocker(eraserCurrentLayer_);
        eraserSize_->setValue(static_cast<int>(eraser_.radius));
        eraserOpacity_->setValue(static_cast<int>(eraser_.opacity * 100.0));
        eraserAlphaOnly_->setChecked(eraser_.eraseAlphaOnly);
        eraserCurrentLayer_->setChecked(eraser_.eraseCurrentLayerOnly);
    }
}

void ToolOptionsPanel::setFillSettings(const FillSettings &settings)
{
    fill_ = settings;
    if (fillTolerance_) {
        QSignalBlocker toleranceBlocker(fillTolerance_);
        QSignalBlocker contiguousBlocker(fillContiguous_);
        QSignalBlocker sampleAllBlocker(fillSampleAll_);
        QSignalBlocker opacityBlocker(fillOpacity_);
        fillTolerance_->setValue(fill_.tolerance);
        fillContiguous_->setChecked(fill_.contiguous);
        fillSampleAll_->setChecked(fill_.sampleAllLayers);
        fillOpacity_->setValue(static_cast<int>(fill_.opacity * 100.0));
    }
}

void ToolOptionsPanel::setTextSettings(const TextSettings &settings)
{
    text_ = settings;
    if (text_.font.pointSize() <= 0) {
        text_.font.setPointSize(32);
    }
    if (text_.boxSize.width() < 32.0 || text_.boxSize.height() < 32.0) {
        text_.boxSize = QSizeF(480.0, 220.0);
    }
    if (textPointSize_) {
        QSignalBlocker sizeBlocker(textPointSize_);
        QSignalBlocker widthBlocker(textBoxWidth_);
        QSignalBlocker heightBlocker(textBoxHeight_);
        textPointSize_->setValue(text_.font.pointSize());
        textBoxWidth_->setValue(static_cast<int>(text_.boxSize.width()));
        textBoxHeight_->setValue(static_cast<int>(text_.boxSize.height()));
    }
}

void ToolOptionsPanel::setActiveTool(ToolType type)
{
    stack_->setCurrentIndex(static_cast<int>(type));
}

void ToolOptionsPanel::setSelectionBounds(const QRect &selection)
{
    if (!selectionWidth_) {
        return;
    }
    const bool valid = selection.isValid() && selection.width() > 0 && selection.height() > 0;
    selectionWidth_->setEnabled(valid);
    selectionHeight_->setEnabled(valid);
    selectionRotation_->setEnabled(valid);
    selectionScale_->setEnabled(valid);
    selectionApplyTransform_->setEnabled(valid);
    if (!valid) {
        return;
    }
    selectionWidth_->setValue(selection.width());
    selectionHeight_->setValue(selection.height());
    selectionRotation_->setValue(0.0);
    selectionScale_->setValue(100);
}

QWidget *ToolOptionsPanel::buildBrushPage()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    brushSize_ = new QSpinBox(page);
    brushSize_->setObjectName(QStringLiteral("BrushSize"));
    brushSize_->setRange(1, 512);
    brushOpacity_ = new QSpinBox(page);
    brushOpacity_->setObjectName(QStringLiteral("BrushOpacity"));
    brushOpacity_->setRange(1, 100);
    brushFlow_ = new QSpinBox(page);
    brushFlow_->setObjectName(QStringLiteral("BrushFlow"));
    brushFlow_->setRange(1, 100);
    brushHardness_ = new QSpinBox(page);
    brushHardness_->setObjectName(QStringLiteral("BrushHardness"));
    brushHardness_->setRange(0, 100);
    brushSpacing_ = new QDoubleSpinBox(page);
    brushSpacing_->setObjectName(QStringLiteral("BrushSpacing"));
    brushSpacing_->setRange(0.05, 1.0);
    brushSpacing_->setSingleStep(0.05);
    brushPressureSize_ = new QCheckBox(QStringLiteral("Pressure controls size"), page);
    brushPressureSize_->setObjectName(QStringLiteral("BrushPressureSize"));
    brushPressureOpacity_ = new QCheckBox(QStringLiteral("Pressure controls opacity"), page);
    brushPressureOpacity_->setObjectName(QStringLiteral("BrushPressureOpacity"));
    form->addRow(QStringLiteral("Size"), brushSize_);
    form->addRow(QStringLiteral("Opacity"), brushOpacity_);
    form->addRow(QStringLiteral("Flow"), brushFlow_);
    form->addRow(QStringLiteral("Hardness"), brushHardness_);
    form->addRow(QStringLiteral("Spacing"), brushSpacing_);
    form->addRow(QString(), brushPressureSize_);
    form->addRow(QString(), brushPressureOpacity_);
    QObject::connect(brushSize_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitBrush);
    QObject::connect(brushOpacity_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitBrush);
    QObject::connect(brushFlow_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitBrush);
    QObject::connect(brushHardness_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitBrush);
    QObject::connect(brushSpacing_, &QDoubleSpinBox::valueChanged, this, &ToolOptionsPanel::emitBrush);
    QObject::connect(brushPressureSize_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitBrush);
    QObject::connect(brushPressureOpacity_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitBrush);
    setBrushSettings(brush_);
    return page;
}

QWidget *ToolOptionsPanel::buildEraserPage()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    eraserSize_ = new QSpinBox(page);
    eraserSize_->setRange(1, 512);
    eraserOpacity_ = new QSpinBox(page);
    eraserOpacity_->setRange(1, 100);
    eraserAlphaOnly_ = new QCheckBox(QStringLiteral("Erase alpha only"), page);
    eraserCurrentLayer_ = new QCheckBox(QStringLiteral("Erase current layer"), page);
    eraserCurrentLayer_->setChecked(true);
    eraserCurrentLayer_->setEnabled(false);
    eraserCurrentLayer_->setToolTip(QStringLiteral("HXPainter erases only the active layer so lower layers are preserved."));
    form->addRow(QStringLiteral("Size"), eraserSize_);
    form->addRow(QStringLiteral("Opacity"), eraserOpacity_);
    form->addRow(QString(), eraserAlphaOnly_);
    form->addRow(QString(), eraserCurrentLayer_);
    QObject::connect(eraserSize_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitEraser);
    QObject::connect(eraserOpacity_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitEraser);
    QObject::connect(eraserAlphaOnly_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitEraser);
    QObject::connect(eraserCurrentLayer_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitEraser);
    setEraserSettings(eraser_);
    return page;
}

QWidget *ToolOptionsPanel::buildFillPage()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    fillTolerance_ = new QSpinBox(page);
    fillTolerance_->setRange(0, 255);
    fillContiguous_ = new QCheckBox(QStringLiteral("Contiguous"), page);
    fillSampleAll_ = new QCheckBox(QStringLiteral("Sample all layers"), page);
    fillOpacity_ = new QSpinBox(page);
    fillOpacity_->setRange(1, 100);
    form->addRow(QStringLiteral("Tolerance"), fillTolerance_);
    form->addRow(QString(), fillContiguous_);
    form->addRow(QString(), fillSampleAll_);
    form->addRow(QStringLiteral("Opacity"), fillOpacity_);
    QObject::connect(fillTolerance_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitFill);
    QObject::connect(fillContiguous_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitFill);
    QObject::connect(fillSampleAll_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitFill);
    QObject::connect(fillOpacity_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitFill);
    setFillSettings(fill_);
    return page;
}

QWidget *ToolOptionsPanel::buildSelectionPage()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    selectionMode_ = new QComboBox(page);
    selectionMode_->addItems({QStringLiteral("Replace"), QStringLiteral("Add"), QStringLiteral("Subtract"), QStringLiteral("Intersect")});
    selectionFeather_ = new QSpinBox(page);
    selectionFeather_->setRange(0, 256);
    selectionAntialias_ = new QCheckBox(QStringLiteral("Anti-aliasing"), page);
    selectionAntialias_->setChecked(true);
    form->addRow(QStringLiteral("Mode"), selectionMode_);
    form->addRow(QStringLiteral("Feather"), selectionFeather_);
    form->addRow(QString(), selectionAntialias_);
    QObject::connect(selectionMode_, &QComboBox::currentTextChanged, this, &ToolOptionsPanel::emitSelection);
    QObject::connect(selectionFeather_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitSelection);
    QObject::connect(selectionAntialias_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitSelection);
    return page;
}

QWidget *ToolOptionsPanel::buildTransformPage()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    selectionRotation_ = new QDoubleSpinBox(page);
    selectionRotation_->setRange(-360.0, 360.0);
    selectionRotation_->setSingleStep(5.0);
    selectionRotation_->setSuffix(QStringLiteral(" deg"));
    selectionWidth_ = new QSpinBox(page);
    selectionWidth_->setRange(1, 8192);
    selectionHeight_ = new QSpinBox(page);
    selectionHeight_->setRange(1, 8192);
    selectionScale_ = new QSpinBox(page);
    selectionScale_->setRange(1, 1000);
    selectionScale_->setValue(100);
    selectionScale_->setSuffix(QStringLiteral("%"));
    selectionApplyTransform_ = new QPushButton(QStringLiteral("Apply Transform"), page);
    form->addRow(QStringLiteral("Rotation"), selectionRotation_);
    form->addRow(QStringLiteral("Width"), selectionWidth_);
    form->addRow(QStringLiteral("Height"), selectionHeight_);
    form->addRow(QStringLiteral("Scale"), selectionScale_);
    form->addRow(QString(), selectionApplyTransform_);
    QObject::connect(selectionApplyTransform_, &QPushButton::clicked, this, &ToolOptionsPanel::emitSelectionTransform);
    setSelectionBounds(QRect());
    return page;
}

QWidget *ToolOptionsPanel::buildShapePage()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    shapeStrokeWidth_ = new QSpinBox(page);
    shapeStrokeWidth_->setRange(1, 256);
    shapeStrokeWidth_->setValue(2);
    shapeFillEnabled_ = new QCheckBox(QStringLiteral("Fill enabled"), page);
    form->addRow(QStringLiteral("Stroke Width"), shapeStrokeWidth_);
    form->addRow(QString(), shapeFillEnabled_);
    form->addRow(QStringLiteral("Stroke Color"), new QLabel(QStringLiteral("Uses foreground color"), page));
    form->addRow(QStringLiteral("Fill Color"), new QLabel(QStringLiteral("Uses background color"), page));
    QObject::connect(shapeStrokeWidth_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitShape);
    QObject::connect(shapeFillEnabled_, &QCheckBox::toggled, this, &ToolOptionsPanel::emitShape);
    return page;
}

QWidget *ToolOptionsPanel::buildTextPage()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    textPointSize_ = new QSpinBox(page);
    textPointSize_->setRange(4, 256);
    textBoxWidth_ = new QSpinBox(page);
    textBoxWidth_->setRange(32, 8192);
    textBoxHeight_ = new QSpinBox(page);
    textBoxHeight_->setRange(32, 8192);
    form->addRow(QStringLiteral("Size"), textPointSize_);
    form->addRow(QStringLiteral("Box Width"), textBoxWidth_);
    form->addRow(QStringLiteral("Box Height"), textBoxHeight_);
    form->addRow(QStringLiteral("Color"), new QLabel(QStringLiteral("Uses foreground RGBA"), page));
    QObject::connect(textPointSize_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitText);
    QObject::connect(textBoxWidth_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitText);
    QObject::connect(textBoxHeight_, &QSpinBox::valueChanged, this, &ToolOptionsPanel::emitText);
    setTextSettings(text_);
    return page;
}

QWidget *ToolOptionsPanel::buildPlaceholderPage(const QString &message)
{
    QLabel *label = new QLabel(message, this);
    label->setWordWrap(true);
    return label;
}

void ToolOptionsPanel::emitBrush()
{
    brush_.radius = brushSize_->value();
    brush_.opacity = brushOpacity_->value() / 100.0;
    brush_.flow = brushFlow_->value() / 100.0;
    brush_.hardness = brushHardness_->value() / 100.0;
    brush_.spacing = brushSpacing_->value();
    brush_.pressureControlsRadius = brushPressureSize_->isChecked();
    brush_.pressureControlsOpacity = brushPressureOpacity_->isChecked();
    Q_EMIT brushSettingsChanged(brush_);
}

void ToolOptionsPanel::emitEraser()
{
    eraser_.radius = eraserSize_->value();
    eraser_.opacity = eraserOpacity_->value() / 100.0;
    eraser_.eraseAlphaOnly = eraserAlphaOnly_->isChecked();
    eraser_.eraseCurrentLayerOnly = true;
    Q_EMIT eraserSettingsChanged(eraser_);
}

void ToolOptionsPanel::emitFill()
{
    fill_.tolerance = fillTolerance_->value();
    fill_.contiguous = fillContiguous_->isChecked();
    fill_.sampleAllLayers = fillSampleAll_->isChecked();
    fill_.opacity = fillOpacity_->value() / 100.0;
    Q_EMIT fillSettingsChanged(fill_);
}

void ToolOptionsPanel::emitText()
{
    QFont font = text_.font;
    font.setPointSize(textPointSize_->value());
    text_.font = font;
    text_.boxSize = QSizeF(textBoxWidth_->value(), textBoxHeight_->value());
    Q_EMIT textSettingsChanged(text_);
}

void ToolOptionsPanel::emitSelection()
{
    Q_EMIT selectionSettingsChanged(selectionMode_->currentText(), selectionFeather_->value(), selectionAntialias_->isChecked());
}

void ToolOptionsPanel::emitSelectionTransform()
{
    Q_EMIT selectionTransformRequested(selectionRotation_->value(),
                                       selectionWidth_->value(),
                                       selectionHeight_->value(),
                                       selectionScale_->value());
}

void ToolOptionsPanel::emitShape()
{
    Q_EMIT shapeSettingsChanged(shapeStrokeWidth_->value(), shapeFillEnabled_->isChecked());
}

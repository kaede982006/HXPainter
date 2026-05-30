#include "MainWindow.h"

#include "app/AppPaths.h"
#include "commands/Command.h"
#include "export/ExportManager.h"
#include "serialization/ProjectSerializer.h"
#include "ui/CreateOpenHub.h"
#include "ui/ExportDialog.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCursor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabletEvent>
#include <QToolBar>
#include <QTouchEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
void configureIconButton(QPushButton *button, const QString &iconPath, const QString &tooltip)
{
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tooltip);
    button->setStatusTip(tooltip);
    button->setText(QString());
    button->setFixedSize(32, 30);
}

QString swatchStyle(const QColor &color)
{
    const int luma = static_cast<int>((0.299 * color.red()) + (0.587 * color.green()) + (0.114 * color.blue()));
    const QString textColor = luma > 150 && color.alpha() > 180 ? QStringLiteral("#11151c") : QStringLiteral("#f6f8fb");
    return QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid #607086; border-radius: 6px; padding: 6px 10px; }"
        "QPushButton:hover { border-color: #9ecbff; }")
        .arg(color.name(QColor::HexArgb), textColor);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("HXPainterMainWindow"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks);

    actions_ = new ActionRegistry(this);
    toolManager_ = new ToolManager(this);
    canvas_ = new OpenGLCanvasWidget(this);
    canvas_->setToolManager(toolManager_);
    setCentralWidget(canvas_);
    qApp->installEventFilter(this);

    buildMenus();
    buildToolBars();
    buildDocks();
    buildStatusBar();
    connectActions();
    connectDocumentSignals();

    QObject::connect(&diagnosticsTimer_, &QTimer::timeout, this, [this] {
        refreshDiagnostics();
        refreshStatus();
    });
    diagnosticsTimer_.start(250);

    setWindowTitle(QStringLiteral("HXPainter"));
    if (!QApplication::windowIcon().isNull()) {
        setWindowIcon(QApplication::windowIcon());
    }

    refreshUiState();
    updateColorControls(canvas_->foregroundColor());
    backgroundButton_->setStyleSheet(swatchStyle(canvas_->backgroundColor()));

    // Create a default empty document on startup instead of blocking the UI thread with a modal dialog.
    // Showing a modal dialog (QDialog::exec) during or immediately after window creation 
    // causes the Windows QOpenGLWidget to deadlock during its initial context creation.
    QTimer::singleShot(0, this, [this] {
        if (!canvas_->hasDocument()) {
            NewDocumentSettings settings;
            settings.width = 1920;
            settings.height = 1080;
            settings.backgroundColor = Qt::white;
            settings.defaultLayerCount = 1;
            settings.templateName = QStringLiteral("Default Canvas");
            canvas_->newDocument(settings);
            startupHubShown_ = true;
        }
    });
}

void MainWindow::setStartupDiagnostics(const QStringList &messages)
{
    startupDiagnostics_ = messages;
    refreshDiagnostics();
    if (!messages.isEmpty()) {
        statusBar()->showMessage(messages.last(), 4000);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSaveModified()) {
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ApplicationActivate || event->type() == QEvent::WindowActivate) {
        focusCanvasIfPointerOverCanvas();
    } else if (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseMove
        || event->type() == QEvent::MouseButtonRelease) {
        if (routeCanvasMouseEvent(watched, static_cast<QMouseEvent *>(event))) {
            return true;
        }
    } else if (event->type() == QEvent::TabletPress
        || event->type() == QEvent::TabletMove
        || event->type() == QEvent::TabletRelease) {

        if (event->type() == QEvent::TabletPress) {
            if (QWidget *w = qobject_cast<QWidget *>(watched)) {
                if (w->focusPolicy() & Qt::ClickFocus) {
                    w->setFocus(Qt::TabFocusReason);
                }
            }
        }

        if (routeCanvasTabletEvent(watched, static_cast<QTabletEvent *>(event))) {
            return true;
        }
    } else if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchUpdate
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchCancel) {
        if (routeCanvasTouchEvent(watched, static_cast<QTouchEvent *>(event))) {
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::canvasContainsGlobalPosition(const QPointF &globalPosition, QPointF *canvasPosition) const
{
    if (!canvas_ || !canvas_->isVisible()) {
        return false;
    }
    if (!std::isfinite(globalPosition.x()) || !std::isfinite(globalPosition.y())) {
        return false;
    }

    const QPointF localPosition = canvas_->mapFromGlobal(globalPosition);
    if (canvasPosition) {
        *canvasPosition = localPosition;
    }
    return QRectF(canvas_->rect()).contains(localPosition);
}

void MainWindow::focusCanvasIfPointerOverCanvas()
{
    if (shouldBlockCanvasRedirect() || !isActiveWindow()) {
        return;
    }
    if (canvasContainsGlobalPosition(QCursor::pos())) {
        canvas_->setFocus(Qt::OtherFocusReason);
    }
}

bool MainWindow::shouldBlockCanvasRedirect() const
{
    return shouldBlockCanvasRedirect(QCursor::pos());
}

bool MainWindow::shouldBlockCanvasRedirect(const QPointF &globalPos) const
{
    if (QApplication::activeModalWidget() != nullptr
        || QApplication::activePopupWidget() != nullptr) {
        return true;
    }

    // If the pointer is physically over the canvas, we should generally ALLOW redirect
    // to break sticky focus from docks.
    if (canvasContainsGlobalPosition(globalPos)) {
        return false;
    }

    QWidget *focusWidget = QApplication::focusWidget();
    if (!focusWidget) {
        return false;
    }

    return (toolOptionsDock_->isAncestorOf(focusWidget)
         || layersDock_->isAncestorOf(focusWidget)
         || colorDock_->isAncestorOf(focusWidget)
         || brushPresetsDock_->isAncestorOf(focusWidget)
         || historyDock_->isAncestorOf(focusWidget)
         || navigatorDock_->isAncestorOf(focusWidget)
         || performanceDock_->isAncestorOf(focusWidget));
}

bool MainWindow::routeCanvasMouseEvent(QObject *target, QMouseEvent *event)
{
    if (!canvas_ || !event) {
        return false;
    }

    QPointF canvasPosition;
    const bool overCanvas = canvasContainsGlobalPosition(event->globalPosition(), &canvasPosition);

    // If the user physically clicks on the canvas area, aggressively take focus.
    // This solves "sticky focus" where a dock widget keeps focus and steals shortcuts.
    if ((event->type() == QEvent::MouseButtonPress || (event->type() == QEvent::MouseMove && event->buttons().testFlag(Qt::LeftButton))) && overCanvas) {
        if (!QApplication::activeModalWidget() && !QApplication::activePopupWidget()) {
            if (QWidget *fw = QApplication::focusWidget()) {
                if (fw != canvas_ && !canvas_->isAncestorOf(fw)) {
                    fw->clearFocus();
                }
            }
            canvas_->setFocus(Qt::MouseFocusReason);
        }
    }

    if (shouldBlockCanvasRedirect(event->globalPosition())) {
        mouseCanvasRedirectActive_ = false;
        return false;
    }

    if (QWidget *targetWidget = qobject_cast<QWidget *>(target)) {
        if (targetWidget == canvas_ || canvas_->isAncestorOf(targetWidget)) {
            if (event->type() == QEvent::MouseButtonRelease) {
                mouseCanvasRedirectActive_ = false;
            }
            return false;
        }
    }

    const bool primaryInvolved = event->button() == Qt::LeftButton
        || event->buttons().testFlag(Qt::LeftButton);
    if (!primaryInvolved && !mouseCanvasRedirectActive_) {
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress) {
        mouseCanvasRedirectActive_ = overCanvas && event->button() == Qt::LeftButton;
        if (!mouseCanvasRedirectActive_) {
            return false;
        }
    } else if (event->type() == QEvent::MouseMove) {
        if (!mouseCanvasRedirectActive_ && overCanvas && event->buttons().testFlag(Qt::LeftButton)) {
            mouseCanvasRedirectActive_ = true;
            canvas_->setFocus(Qt::MouseFocusReason);
        }
        if (!mouseCanvasRedirectActive_) {
            return false;
        }
    } else if (!mouseCanvasRedirectActive_) {
        return false;
    }

    QMouseEvent forwarded(event->type(),
                          canvasPosition,
                          canvasPosition,
                          event->globalPosition(),
                          event->button(),
                          event->buttons(),
                          event->modifiers(),
                          event->source(),
                          event->pointingDevice());
    QApplication::sendEvent(canvas_, &forwarded);
    event->accept();

    if (event->type() == QEvent::MouseButtonRelease) {
        mouseCanvasRedirectActive_ = false;
    }
    return true;
}

bool MainWindow::routeCanvasTabletEvent(QObject *target, QTabletEvent *event)
{
    if (!canvas_ || !event) {
        return false;
    }

    QPointF canvasPosition;
    const bool overCanvas = canvasContainsGlobalPosition(event->globalPosition(), &canvasPosition);

    // Aggressively grab focus if tablet physically hits the canvas.
    // This breaks "sticky focus" from tools/layers/panels.
    if ((event->type() == QEvent::TabletPress || (event->type() == QEvent::TabletMove && event->buttons().testFlag(Qt::LeftButton))) && overCanvas) {
        if (!QApplication::activeModalWidget() && !QApplication::activePopupWidget()) {
            if (QWidget *fw = QApplication::focusWidget()) {
                if (fw != canvas_ && !canvas_->isAncestorOf(fw)) {
                    fw->clearFocus();
                }
            }
            canvas_->setFocus(Qt::MouseFocusReason);
        }
    }

    if (shouldBlockCanvasRedirect(event->globalPosition())) {
        tabletCanvasRedirectActive_ = false;
        return false;
    }

    if (QWidget *targetWidget = qobject_cast<QWidget *>(target)) {
        if (targetWidget == canvas_ || canvas_->isAncestorOf(targetWidget)) {
            if (event->type() == QEvent::TabletRelease) {
                tabletCanvasRedirectActive_ = false;
            }
            return false;
        }
    }

    const bool primaryInvolved = event->button() == Qt::LeftButton
        || event->buttons().testFlag(Qt::LeftButton);
    if (event->type() == QEvent::TabletPress) {
        tabletCanvasRedirectActive_ = overCanvas && primaryInvolved;
        if (!tabletCanvasRedirectActive_) {
            return false;
        }
    } else if (event->type() == QEvent::TabletMove) {
        if (!tabletCanvasRedirectActive_ && overCanvas && event->buttons().testFlag(Qt::LeftButton)) {
            tabletCanvasRedirectActive_ = true;
            canvas_->setFocus(Qt::MouseFocusReason);
        }
        if (!tabletCanvasRedirectActive_) {
            return false;
        }
    } else if (!tabletCanvasRedirectActive_) {
        return false;
    }

    QTabletEvent forwarded(event->type(),
                           event->pointingDevice(),
                           canvasPosition,
                           event->globalPosition(),
                           event->pressure(),
                           static_cast<float>(event->xTilt()),
                           static_cast<float>(event->yTilt()),
                           static_cast<float>(event->tangentialPressure()),
                           event->rotation(),
                           static_cast<float>(event->z()),
                           event->modifiers(),
                           event->button(),
                           event->buttons());
    QApplication::sendEvent(canvas_, &forwarded);
    event->accept();

    if (event->type() == QEvent::TabletRelease) {
        tabletCanvasRedirectActive_ = false;
    }
    return true;
}

bool MainWindow::routeCanvasTouchEvent(QObject *target, QTouchEvent *event)
{
    if (!canvas_ || !event) {
        return false;
    }

    QPointF globalPosition;
    if (event->points().isEmpty()) {
        if (!touchCanvasRedirectActive_ || event->type() != QEvent::TouchCancel) {
            return false;
        }
        globalPosition = QCursor::pos();
    } else {
        globalPosition = event->points().first().globalPosition();
    }

    QPointF canvasPosition;
    const bool overCanvas = canvasContainsGlobalPosition(globalPosition, &canvasPosition);

    // Aggressively grab focus if touch physically hits the canvas.
    if ((event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate) && overCanvas) {
        if (!QApplication::activeModalWidget() && !QApplication::activePopupWidget()) {
            if (QWidget *fw = QApplication::focusWidget()) {
                if (fw != canvas_ && !canvas_->isAncestorOf(fw)) {
                    fw->clearFocus();
                }
            }
            canvas_->setFocus(Qt::MouseFocusReason);
        }
    }

    if (shouldBlockCanvasRedirect(globalPosition)) {
        touchCanvasRedirectActive_ = false;
        return false;
    }

    if (event->type() == QEvent::TouchBegin) {
        touchCanvasRedirectActive_ = overCanvas;
        if (!touchCanvasRedirectActive_) {
            return false;
        }
    } else if (event->type() == QEvent::TouchUpdate) {
        if (!touchCanvasRedirectActive_ && overCanvas) {
            touchCanvasRedirectActive_ = true;
            canvas_->setFocus(Qt::MouseFocusReason);
        }
        if (!touchCanvasRedirectActive_) {
            return false;
        }
    } else if (!touchCanvasRedirectActive_) {
        return false;
    }

    QEvent::Type mouseType = QEvent::MouseMove;
    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::LeftButton;
    if (event->type() == QEvent::TouchBegin) {
        mouseType = QEvent::MouseButtonPress;
        button = Qt::LeftButton;
    } else if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
        mouseType = QEvent::MouseButtonRelease;
        button = Qt::LeftButton;
        buttons = Qt::NoButton;
    }

    QMouseEvent forwarded(mouseType,
                          canvasPosition,
                          canvasPosition,
                          globalPosition,
                          button,
                          buttons,
                          event->modifiers(),
                          Qt::MouseEventSynthesizedByQt,
                          event->pointingDevice());
    QApplication::sendEvent(canvas_, &forwarded);
    event->accept();

    if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
        touchCanvasRedirectActive_ = false;
    }
    return true;
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(actions_->newDocument);
    fileMenu->addAction(actions_->open);
    fileMenu->addAction(actions_->importImage);
    fileMenu->addSeparator();
    fileMenu->addAction(actions_->save);
    fileMenu->addAction(actions_->saveAs);
    fileMenu->addAction(actions_->exportImage);
    fileMenu->addSeparator();
    fileMenu->addAction(actions_->closeDocument);
    fileMenu->addAction(actions_->exit);

    QMenu *editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(actions_->undo);
    editMenu->addAction(actions_->redo);
    editMenu->addSeparator();
    editMenu->addAction(actions_->cut);
    editMenu->addAction(actions_->copy);
    editMenu->addAction(actions_->paste);
    editMenu->addAction(actions_->clear);
    editMenu->addSeparator();
    editMenu->addAction(actions_->preferences);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(actions_->zoomIn);
    viewMenu->addAction(actions_->zoomOut);
    viewMenu->addAction(actions_->fitCanvas);
    viewMenu->addAction(actions_->rotateLeft);
    viewMenu->addAction(actions_->rotateRight);
    viewMenu->addAction(actions_->resetView);

    QMenu *canvasMenu = menuBar()->addMenu(QStringLiteral("&Canvas"));
    canvasMenu->addAction(actions_->fitCanvas);
    canvasMenu->addAction(actions_->rotateLeft);
    canvasMenu->addAction(actions_->rotateRight);
    canvasMenu->addAction(actions_->resetView);

    QMenu *layerMenu = menuBar()->addMenu(QStringLiteral("&Layer"));
    layerMenu->addAction(actions_->addLayer);
    layerMenu->addAction(actions_->deleteLayer);
    layerMenu->addAction(actions_->duplicateLayer);
    layerMenu->addAction(actions_->renameLayer);
    layerMenu->addSeparator();
    layerMenu->addAction(actions_->moveLayerUp);
    layerMenu->addAction(actions_->moveLayerDown);
    layerMenu->addAction(actions_->mergeLayerDown);
    layerMenu->addSeparator();
    layerMenu->addAction(actions_->toggleLayerVisibility);
    layerMenu->addAction(actions_->toggleLayerLock);
    layerMenu->addAction(actions_->layerOpacity);
    layerMenu->addAction(actions_->blendMode);

    QMenu *selectMenu = menuBar()->addMenu(QStringLiteral("&Select"));
    selectMenu->addAction(actions_->selection);
    selectMenu->addAction(actions_->deselect);
    selectMenu->addSeparator();
    selectMenu->addAction(actions_->clear);

    QMenu *filterMenu = menuBar()->addMenu(QStringLiteral("Fi&lter"));
    filterMenu->addAction(actions_->filterGallery);

    windowMenu_ = menuBar()->addMenu(QStringLiteral("&Window"));

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("About HXPainter"), this, [this] {
        QMessageBox::about(this, QStringLiteral("HXPainter"),
                           QStringLiteral("HXPainter MVP\nQt painting program with tools, layers, commands, and deployment-ready assets."));
    });
}

void MainWindow::buildToolBars()
{
    QToolBar *mainToolBar = addToolBar(QStringLiteral("Main"));
    mainToolBar->setObjectName(QStringLiteral("MainToolBar"));
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(20, 20));
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mainToolBar->addAction(actions_->newDocument);
    mainToolBar->addAction(actions_->open);
    mainToolBar->addAction(actions_->save);
    mainToolBar->addAction(actions_->exportImage);
    mainToolBar->addSeparator();
    mainToolBar->addAction(actions_->undo);
    mainToolBar->addAction(actions_->redo);
    mainToolBar->addSeparator();
    mainToolBar->addAction(actions_->zoomIn);
    mainToolBar->addAction(actions_->zoomOut);
    mainToolBar->addAction(actions_->fitCanvas);
    mainToolBar->addAction(actions_->rotateLeft);
    mainToolBar->addAction(actions_->rotateRight);

    QToolBar *toolBar = new QToolBar(QStringLiteral("Tools"), this);
    toolBar->setObjectName(QStringLiteral("LeftToolBar"));
    toolBar->setMovable(false);
    toolBar->setAllowedAreas(Qt::LeftToolBarArea);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->setOrientation(Qt::Vertical);
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    for (QAction *action : actions_->sideToolActions()) {
        toolBar->addAction(action);
    }
    addToolBar(Qt::LeftToolBarArea, toolBar);
}

void MainWindow::buildDocks()
{
    toolOptionsDock_ = new QDockWidget(QStringLiteral("Tool Options"), this);
    toolOptionsDock_->setObjectName(QStringLiteral("ToolOptionsDock"));
    toolOptionsPanel_ = new ToolOptionsPanel(toolOptionsDock_);
    toolOptionsPanel_->setBrushSettings(canvas_->brushSettings());
    toolOptionsPanel_->setEraserSettings(canvas_->eraserSettings());
    toolOptionsPanel_->setFillSettings(canvas_->fillSettings());
    toolOptionsPanel_->setTextSettings(canvas_->textSettings());
    toolOptionsDock_->setWidget(toolOptionsPanel_);
    addDockWidget(Qt::RightDockWidgetArea, toolOptionsDock_);

    layersDock_ = new QDockWidget(QStringLiteral("Layers"), this);
    layersDock_->setObjectName(QStringLiteral("LayersDock"));
    QWidget *layersWidget = new QWidget(layersDock_);
    layersWidget->setObjectName(QStringLiteral("LayersPanel"));
    layersList_ = new QListWidget(layersWidget);
    layersList_->setObjectName(QStringLiteral("LayersList"));
    QPushButton *addLayer = new QPushButton(QStringLiteral("+"), layersWidget);
    QPushButton *deleteLayer = new QPushButton(QStringLiteral("-"), layersWidget);
    QPushButton *duplicateLayer = new QPushButton(QStringLiteral("Duplicate"), layersWidget);
    QPushButton *mergeLayer = new QPushButton(QStringLiteral("Merge"), layersWidget);
    QPushButton *renameLayer = new QPushButton(QStringLiteral("Rename"), layersWidget);
    QPushButton *visibleLayer = new QPushButton(QStringLiteral("Eye"), layersWidget);
    QPushButton *lockLayer = new QPushButton(QStringLiteral("Lock"), layersWidget);
    QPushButton *alphaLockLayer = new QPushButton(QStringLiteral("Alpha"), layersWidget);
    configureIconButton(addLayer, QStringLiteral(":/icons/plus.svg"), QStringLiteral("Add layer"));
    configureIconButton(deleteLayer, QStringLiteral(":/icons/minus.svg"), QStringLiteral("Delete active layer"));
    configureIconButton(duplicateLayer, QStringLiteral(":/icons/copy-plus.svg"), QStringLiteral("Duplicate active layer"));
    configureIconButton(mergeLayer, QStringLiteral(":/icons/git-merge.svg"), QStringLiteral("Merge active layer down"));
    configureIconButton(renameLayer, QStringLiteral(":/icons/letter-t.svg"), QStringLiteral("Rename active layer"));
    configureIconButton(visibleLayer, QStringLiteral(":/icons/eye.svg"), QStringLiteral("Toggle active layer visibility"));
    configureIconButton(lockLayer, QStringLiteral(":/icons/lock.svg"), QStringLiteral("Toggle active layer lock"));
    configureIconButton(alphaLockLayer, QStringLiteral(":/icons/droplet.svg"), QStringLiteral("Toggle active layer alpha lock"));
    layerOpacitySlider_ = new QSlider(Qt::Horizontal, layersWidget);
    layerOpacitySlider_->setRange(0, 100);
    layerOpacitySlider_->setTracking(false);
    blendModeCombo_ = new QComboBox(layersWidget);
    blendModeCombo_->addItems({QStringLiteral("Normal"), QStringLiteral("Multiply"), QStringLiteral("Screen"), QStringLiteral("Overlay")});

    QHBoxLayout *layerButtons = new QHBoxLayout;
    layerButtons->addWidget(addLayer);
    layerButtons->addWidget(deleteLayer);
    layerButtons->addWidget(duplicateLayer);
    layerButtons->addWidget(mergeLayer);
    QHBoxLayout *layerButtons2 = new QHBoxLayout;
    layerButtons2->addWidget(renameLayer);
    layerButtons2->addWidget(visibleLayer);
    layerButtons2->addWidget(lockLayer);
    layerButtons2->addWidget(alphaLockLayer);
    QFormLayout *layerOptions = new QFormLayout;
    layerOptions->addRow(QStringLiteral("Opacity"), layerOpacitySlider_);
    layerOptions->addRow(QStringLiteral("Blend"), blendModeCombo_);
    QVBoxLayout *layersLayout = new QVBoxLayout(layersWidget);
    layersLayout->addWidget(layersList_);
    layersLayout->addLayout(layerButtons);
    layersLayout->addLayout(layerButtons2);
    layersLayout->addLayout(layerOptions);
    layersDock_->setWidget(layersWidget);
    addDockWidget(Qt::RightDockWidgetArea, layersDock_);

    colorDock_ = new QDockWidget(QStringLiteral("Color Picker"), this);
    colorDock_->setObjectName(QStringLiteral("ColorDock"));
    QWidget *colorWidget = new QWidget(colorDock_);
    foregroundButton_ = new QPushButton(QStringLiteral("Foreground"), colorWidget);
    backgroundButton_ = new QPushButton(QStringLiteral("Background"), colorWidget);
    foregroundButton_->setIcon(QIcon(QStringLiteral(":/icons/palette.svg")));
    backgroundButton_->setIcon(QIcon(QStringLiteral(":/icons/palette.svg")));
    redSpin_ = new QSpinBox(colorWidget);
    greenSpin_ = new QSpinBox(colorWidget);
    blueSpin_ = new QSpinBox(colorWidget);
    alphaSpin_ = new QSpinBox(colorWidget);
    for (QSpinBox *spin : {redSpin_, greenSpin_, blueSpin_, alphaSpin_}) {
        spin->setRange(0, 255);
    }
    QFormLayout *colorLayout = new QFormLayout(colorWidget);
    colorLayout->addRow(foregroundButton_);
    colorLayout->addRow(backgroundButton_);
    colorLayout->addRow(QStringLiteral("R"), redSpin_);
    colorLayout->addRow(QStringLiteral("G"), greenSpin_);
    colorLayout->addRow(QStringLiteral("B"), blueSpin_);
    colorLayout->addRow(QStringLiteral("A"), alphaSpin_);
    colorDock_->setWidget(colorWidget);
    addDockWidget(Qt::RightDockWidgetArea, colorDock_);

    brushPresetsDock_ = new QDockWidget(QStringLiteral("Brush Presets"), this);
    brushPresetsDock_->setObjectName(QStringLiteral("BrushPresetsDock"));
    brushPresetsList_ = new QListWidget(brushPresetsDock_);
    brushPresetsList_->setObjectName(QStringLiteral("BrushPresetsList"));
    const QStringList presets = {QStringLiteral("Pencil"), QStringLiteral("Ink"), QStringLiteral("Hard Brush"),
                                 QStringLiteral("Soft Brush"), QStringLiteral("Marker"), QStringLiteral("Airbrush")};
    brushPresetsList_->addItems(presets);
    brushPresetsDock_->setWidget(brushPresetsList_);
    addDockWidget(Qt::RightDockWidgetArea, brushPresetsDock_);

    historyDock_ = new QDockWidget(QStringLiteral("History"), this);
    historyDock_->setObjectName(QStringLiteral("HistoryDock"));
    historyList_ = new QListWidget(historyDock_);
    historyDock_->setWidget(historyList_);
    addDockWidget(Qt::RightDockWidgetArea, historyDock_);

    navigatorDock_ = new QDockWidget(QStringLiteral("Navigator"), this);
    navigatorDock_->setObjectName(QStringLiteral("NavigatorDock"));
    navigatorLabel_ = new QLabel(navigatorDock_);
    navigatorLabel_->setMinimumSize(180, 120);
    navigatorLabel_->setAlignment(Qt::AlignCenter);
    navigatorDock_->setWidget(navigatorLabel_);
    addDockWidget(Qt::RightDockWidgetArea, navigatorDock_);

    performanceDock_ = new QDockWidget(QStringLiteral("Performance Monitor"), this);
    performanceDock_->setObjectName(QStringLiteral("PerformanceDock"));
    diagnosticsLabel_ = new QLabel(performanceDock_);
    diagnosticsLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    diagnosticsLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    performanceDock_->setWidget(diagnosticsLabel_);
    addDockWidget(Qt::RightDockWidgetArea, performanceDock_);

    tabifyDockWidget(toolOptionsDock_, layersDock_);
    tabifyDockWidget(layersDock_, colorDock_);
    tabifyDockWidget(colorDock_, brushPresetsDock_);
    toolOptionsDock_->raise();

    if (windowMenu_) {
        windowMenu_->addAction(toolOptionsDock_->toggleViewAction());
        windowMenu_->addAction(layersDock_->toggleViewAction());
        windowMenu_->addAction(colorDock_->toggleViewAction());
        windowMenu_->addAction(brushPresetsDock_->toggleViewAction());
        windowMenu_->addAction(historyDock_->toggleViewAction());
        windowMenu_->addAction(navigatorDock_->toggleViewAction());
        windowMenu_->addAction(performanceDock_->toggleViewAction());
    }

    QObject::connect(addLayer, &QPushButton::clicked, canvas_->layerManager(), &LayerManager::addLayer);
    QObject::connect(deleteLayer, &QPushButton::clicked, canvas_->layerManager(), &LayerManager::deleteActiveLayer);
    QObject::connect(duplicateLayer, &QPushButton::clicked, canvas_->layerManager(), &LayerManager::duplicateActiveLayer);
    QObject::connect(mergeLayer, &QPushButton::clicked, canvas_->layerManager(), &LayerManager::mergeActiveLayerDown);
    QObject::connect(renameLayer, &QPushButton::clicked, this, &MainWindow::renameActiveLayer);
    QObject::connect(visibleLayer, &QPushButton::clicked, this, &MainWindow::toggleActiveLayerVisibility);
    QObject::connect(lockLayer, &QPushButton::clicked, this, &MainWindow::toggleActiveLayerLock);
    QObject::connect(alphaLockLayer, &QPushButton::clicked, this, &MainWindow::toggleActiveLayerAlphaLock);
}

void MainWindow::buildStatusBar()
{
    docSizeStatus_ = new QLabel(this);
    zoomStatus_ = new QLabel(this);
    cursorStatus_ = new QLabel(this);
    toolStatus_ = new QLabel(this);
    layerStatus_ = new QLabel(this);
    brushStatus_ = new QLabel(this);
    colorStatus_ = new QLabel(this);
    layerOpacityStatus_ = new QLabel(this);
    fpsStatus_ = new QLabel(this);
    for (QLabel *label : {docSizeStatus_, zoomStatus_, cursorStatus_, toolStatus_, layerStatus_,
                          brushStatus_, colorStatus_, layerOpacityStatus_, fpsStatus_}) {
        statusBar()->addPermanentWidget(label);
    }
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::connectActions()
{
    QObject::connect(actions_->newDocument, &QAction::triggered, this, &MainWindow::showCreateOpenHub);
    QObject::connect(actions_->open, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open"),
            QString(), QStringLiteral("HXP Project (*.hxp);;Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*.*)"));
        if (!path.isEmpty()) {
            if (!maybeSaveModified()) {
                return;
            }
            openFile(path);
        }
    });
    QObject::connect(actions_->importImage, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Image"),
            QString(), QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*.*)"));
        if (!path.isEmpty()) {
            importFile(path);
        }
    });
    QObject::connect(actions_->save, &QAction::triggered, this, &MainWindow::save);
    QObject::connect(actions_->saveAs, &QAction::triggered, this, &MainWindow::saveAs);
    QObject::connect(actions_->exportImage, &QAction::triggered, this, &MainWindow::exportImage);
    QObject::connect(actions_->closeDocument, &QAction::triggered, this, &MainWindow::closeDocument);
    QObject::connect(actions_->exit, &QAction::triggered, this, &QWidget::close);
    QObject::connect(actions_->undo, &QAction::triggered, canvas_->commandManager(), &CommandManager::undo);
    QObject::connect(actions_->redo, &QAction::triggered, canvas_->commandManager(), &CommandManager::redo);
    QObject::connect(actions_->cut, &QAction::triggered, this, &MainWindow::cutActiveLayer);
    QObject::connect(actions_->copy, &QAction::triggered, this, &MainWindow::copyActiveLayer);
    QObject::connect(actions_->paste, &QAction::triggered, this, &MainWindow::pasteImageFromClipboard);
    QObject::connect(actions_->clear, &QAction::triggered, this, &MainWindow::clearActiveLayer);
    QObject::connect(actions_->deselect, &QAction::triggered, canvas_, &OpenGLCanvasWidget::clearSelection);
    QObject::connect(actions_->preferences, &QAction::triggered, this, &MainWindow::showPreferences);
    QObject::connect(actions_->filterGallery, &QAction::triggered, this, &MainWindow::showFilterGallery);
    QObject::connect(actions_->zoomIn, &QAction::triggered, canvas_, &OpenGLCanvasWidget::zoomIn);
    QObject::connect(actions_->zoomOut, &QAction::triggered, canvas_, &OpenGLCanvasWidget::zoomOut);
    QObject::connect(actions_->fitCanvas, &QAction::triggered, canvas_, &OpenGLCanvasWidget::fitCanvas);
    QObject::connect(actions_->rotateLeft, &QAction::triggered, canvas_, &OpenGLCanvasWidget::rotateViewLeft);
    QObject::connect(actions_->rotateRight, &QAction::triggered, canvas_, &OpenGLCanvasWidget::rotateViewRight);
    QObject::connect(actions_->resetView, &QAction::triggered, canvas_, &OpenGLCanvasWidget::resetView);

    QObject::connect(actions_->addLayer, &QAction::triggered, canvas_->layerManager(), &LayerManager::addLayer);
    QObject::connect(actions_->deleteLayer, &QAction::triggered, canvas_->layerManager(), &LayerManager::deleteActiveLayer);
    QObject::connect(actions_->duplicateLayer, &QAction::triggered, canvas_->layerManager(), &LayerManager::duplicateActiveLayer);
    QObject::connect(actions_->renameLayer, &QAction::triggered, this, &MainWindow::renameActiveLayer);
    QObject::connect(actions_->moveLayerUp, &QAction::triggered, canvas_->layerManager(), &LayerManager::moveActiveLayerUp);
    QObject::connect(actions_->moveLayerDown, &QAction::triggered, canvas_->layerManager(), &LayerManager::moveActiveLayerDown);
    QObject::connect(actions_->mergeLayerDown, &QAction::triggered, canvas_->layerManager(), &LayerManager::mergeActiveLayerDown);
    QObject::connect(actions_->toggleLayerVisibility, &QAction::triggered, this, &MainWindow::toggleActiveLayerVisibility);
    QObject::connect(actions_->toggleLayerLock, &QAction::triggered, this, &MainWindow::toggleActiveLayerLock);
    QObject::connect(actions_->layerOpacity, &QAction::triggered, this, &MainWindow::showLayerOpacityDialog);
    QObject::connect(actions_->blendMode, &QAction::triggered, this, &MainWindow::showBlendModeDialog);

    QObject::connect(actions_->toolGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        toolManager_->setActiveTool(static_cast<ToolType>(action->data().toInt()));
    });
    QObject::connect(toolManager_, &ToolManager::activeToolChanged, toolOptionsPanel_, &ToolOptionsPanel::setActiveTool);
    QObject::connect(toolManager_, &ToolManager::activeToolChanged, this, [this](ToolType type) {
        if (QAction *action = actions_->toolAction(type)) {
            action->setChecked(true);
        }
        refreshStatus();
    });

    QObject::connect(toolOptionsPanel_, &ToolOptionsPanel::brushSettingsChanged, canvas_, &OpenGLCanvasWidget::setBrushSettings);
    QObject::connect(toolOptionsPanel_, &ToolOptionsPanel::eraserSettingsChanged, canvas_, &OpenGLCanvasWidget::setEraserSettings);
    QObject::connect(toolOptionsPanel_, &ToolOptionsPanel::fillSettingsChanged, canvas_, &OpenGLCanvasWidget::setFillSettings);
    QObject::connect(toolOptionsPanel_, &ToolOptionsPanel::textSettingsChanged, canvas_, &OpenGLCanvasWidget::setTextSettings);
    QObject::connect(toolOptionsPanel_, &ToolOptionsPanel::selectionSettingsChanged, canvas_, &OpenGLCanvasWidget::setSelectionSettings);
    QObject::connect(toolOptionsPanel_, &ToolOptionsPanel::selectionTransformRequested, canvas_, &OpenGLCanvasWidget::transformSelection);
    QObject::connect(canvas_, &OpenGLCanvasWidget::selectionChanged, toolOptionsPanel_, &ToolOptionsPanel::setSelectionBounds);
    QObject::connect(toolOptionsPanel_, &ToolOptionsPanel::shapeSettingsChanged, canvas_, &OpenGLCanvasWidget::setShapeSettings);
    QObject::connect(brushPresetsList_, &QListWidget::itemClicked, this, &MainWindow::applyBrushPreset);

    QObject::connect(foregroundButton_, &QPushButton::clicked, this, &MainWindow::chooseForegroundColor);
    QObject::connect(backgroundButton_, &QPushButton::clicked, this, &MainWindow::chooseBackgroundColor);
    for (QSpinBox *spin : {redSpin_, greenSpin_, blueSpin_, alphaSpin_}) {
        QObject::connect(spin, &QSpinBox::valueChanged, this, &MainWindow::setForegroundFromControls);
    }
    QObject::connect(layerOpacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (canvas_->hasDocument()) {
            canvas_->layerManager()->setActiveLayerOpacity(value / 100.0);
        }
    });
    QObject::connect(blendModeCombo_, &QComboBox::currentTextChanged, canvas_->layerManager(), &LayerManager::setActiveLayerBlendMode);
}

void MainWindow::connectDocumentSignals()
{
    QObject::connect(canvas_, &OpenGLCanvasWidget::documentChanged, this, &MainWindow::refreshUiState);
    QObject::connect(canvas_, &OpenGLCanvasWidget::statusMessage, this, &MainWindow::showMessage);
    QObject::connect(canvas_, &OpenGLCanvasWidget::colorPicked, this, [this](const QColor &color) {
        updateColorControls(color);
    });
    QObject::connect(canvas_, &OpenGLCanvasWidget::cursorPositionChanged, this, [this](const QPointF &p) {
        cursorStatus_->setText(QStringLiteral("X %1 Y %2").arg(p.x(), 0, 'f', 0).arg(p.y(), 0, 'f', 0));
    });
    QObject::connect(canvas_, &OpenGLCanvasWidget::zoomChanged, this, [this](double zoom) {
        zoomStatus_->setText(QStringLiteral("%1%").arg(zoom * 100.0, 0, 'f', 0));
    });
    QObject::connect(canvas_->layerManager(), &LayerManager::layersChanged, this, &MainWindow::refreshLayers);
    QObject::connect(canvas_->layerManager(), &LayerManager::layersChanged, this, &MainWindow::refreshStatus);
    QObject::connect(canvas_->layerManager(), &LayerManager::activeLayerChanged, this, [this](int) {
        refreshLayers();
        refreshStatus();
    });
    QObject::connect(canvas_->commandManager(), &CommandManager::stackChanged, this, [this] {
        actions_->setUndoRedoAvailable(canvas_->commandManager()->canUndo(), canvas_->commandManager()->canRedo());
        refreshHistory();
        refreshUiState();
    });
    QObject::connect(canvas_->commandManager(), &CommandManager::commandApplied, this, &MainWindow::showMessage);
    QObject::connect(layersList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        if (!current) {
            return;
        }
        canvas_->layerManager()->setActiveLayer(current->data(Qt::UserRole).toInt());
    });
}

void MainWindow::showCreateOpenHub()
{
    CreateOpenHub hub(this);
    if (hub.exec() != QDialog::Accepted) {
        refreshUiState();
        return;
    }

    if (hub.mode() == CreateOpenHub::Mode::NewDocument) {
        if (!maybeSaveModified()) {
            return;
        }
        canvas_->newDocument(hub.newDocumentSettings());
        showMessage(QStringLiteral("New document created"));
    } else if (hub.mode() == CreateOpenHub::Mode::OpenExisting) {
        if (!maybeSaveModified()) {
            return;
        }
        openFile(hub.selectedFilePath());
    } else if (hub.mode() == CreateOpenHub::Mode::ImportImage) {
        if (hub.importMode() == CreateOpenHub::ImportMode::OpenAsNewDocument) {
            if (!maybeSaveModified()) {
                return;
            }
            openFile(hub.selectedFilePath());
        } else if (hub.importMode() == CreateOpenHub::ImportMode::ImportAsReferenceImage) {
            importReferenceFile(hub.selectedFilePath());
        } else {
            importFile(hub.selectedFilePath());
        }
    }
}

void MainWindow::openFile(const QString &path)
{
    if (path.endsWith(QStringLiteral(".hxp"), Qt::CaseInsensitive)) {
        QString error;
        if (!ProjectSerializer::load(canvas_->document(), path, &error)) {
            QMessageBox::warning(this, QStringLiteral("Open Project"), error);
            return;
        }
        canvas_->notifyDocumentLoaded(path);
        CreateOpenHub::rememberLoadedProject(path);
    } else if (!canvas_->openImage(path)) {
        QMessageBox::warning(this, QStringLiteral("Open Image"), QStringLiteral("Failed to open image."));
        return;
    }
    showMessage(QStringLiteral("Opened %1").arg(path));
}

void MainWindow::importFile(const QString &path)
{
    if (!canvas_->hasDocument()) {
        openFile(path);
        return;
    }
    canvas_->layerManager()->importImageAsLayer(path);
}

void MainWindow::importReferenceFile(const QString &path)
{
    if (!canvas_->hasDocument()) {
        openFile(path);
        return;
    }

    QImage image;
    if (!image.load(path)) {
        QMessageBox::warning(this, QStringLiteral("Import Reference"), QStringLiteral("Failed to load image."));
        return;
    }

    const DocumentState before = canvas_->document().snapshot();
    QString error;
    if (!canvas_->document().addImageLayer(image, QStringLiteral("Reference - %1").arg(QFileInfo(path).completeBaseName()), &error, true)) {
        QMessageBox::warning(this, QStringLiteral("Import Reference"), error);
        return;
    }
    Layer *layer = canvas_->document().activeLayer();
    if (layer) {
        layer->locked = true;
        layer->opacity = 0.6;
    }
    canvas_->commandManager()->execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Import Reference Image"), &canvas_->document(), before, canvas_->document().snapshot()));
    showMessage(QStringLiteral("Imported reference %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::save()
{
    saveDocument();
}

void MainWindow::saveAs()
{
    saveDocumentAs();
}

bool MainWindow::saveDocument()
{
    if (!canvas_->hasDocument()) {
        return false;
    }
    if (canvas_->document().filePath().isEmpty()) {
        return saveDocumentAs();
    }
    QString error;
    if (!ProjectSerializer::save(canvas_->document(), canvas_->document().filePath(), &error)) {
        QMessageBox::warning(this, QStringLiteral("Save"), error);
        return false;
    }
    canvas_->document().setModified(false);
    CreateOpenHub::rememberSavedProject(canvas_->document().filePath());
    showMessage(QStringLiteral("Saved %1").arg(canvas_->document().filePath()));
    refreshUiState();
    return true;
}

bool MainWindow::saveDocumentAs()
{
    if (!canvas_->hasDocument()) {
        return false;
    }
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Project As"),
                                                QStringLiteral("hxpainter-project.hxp"),
                                                QStringLiteral("HXP Project (*.hxp)"));
    if (path.isEmpty()) {
        return false;
    }
    if (!path.endsWith(QStringLiteral(".hxp"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".hxp");
    }
    QString error;
    if (!ProjectSerializer::save(canvas_->document(), path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Save As"), error);
        return false;
    }
    canvas_->document().setFilePath(path);
    canvas_->document().setModified(false);
    CreateOpenHub::rememberSavedProject(path);
    showMessage(QStringLiteral("Saved %1").arg(path));
    refreshUiState();
    return true;
}

bool MainWindow::maybeSaveModified()
{
    if (!canvas_->hasDocument() || !canvas_->document().isModified()) {
        return true;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this,
        QStringLiteral("Unsaved Changes"),
        QStringLiteral("The current document has unsaved changes."),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (choice == QMessageBox::Save) {
        return saveDocument();
    }
    if (choice == QMessageBox::Discard) {
        return true;
    }
    return false;
}

void MainWindow::exportImage()
{
    if (!canvas_ || !canvas_->hasDocument()) {
        return;
    }

    ExportDialog dialog(this);

    // Suggest a filename based on project name in the user's Pictures folder
    QString baseName = QFileInfo(canvas_->document().filePath()).completeBaseName();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Untitled");
    }

    QString picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesDir.isEmpty()) {
        picturesDir = QDir::currentPath();
    }
    QString initialPath = QDir(picturesDir).filePath(baseName + QStringLiteral(".png"));
    dialog.setInitialPath(initialPath);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    ExportOptions options = dialog.options();
    if (options.filePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Export"), QStringLiteral("Choose an export file path."));
        return;
    }
    QString error;
    if (!ExportManager::exportDocument(canvas_->document(), options, &error)) {
        QMessageBox::warning(this, QStringLiteral("Export"), error);
        return;
    }
    showMessage(QStringLiteral("Exported %1").arg(options.filePath));
}

void MainWindow::closeDocument()
{
    if (!maybeSaveModified()) {
        return;
    }
    canvas_->restoreDocument({});
    showMessage(QStringLiteral("Document closed"));
}

void MainWindow::clearActiveLayer()
{
    if (!canvas_->hasDocument()) {
        return;
    }
    const Layer *layer = canvas_->document().activeLayer();
    if (!layer) {
        return;
    }
    if (layer->locked) {
        showMessage(QStringLiteral("Layer is locked"));
        return;
    }

    const QRect target = canvas_->hasSelection() ? canvas_->selectionRect() : QRect();
    const DocumentState before = canvas_->document().snapshot();
    if (!canvas_->document().clearLayer(canvas_->document().activeLayerIndex(), target)) {
        showMessage(QStringLiteral("Nothing to clear"));
        return;
    }
    canvas_->commandManager()->execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Clear"), &canvas_->document(), before, canvas_->document().snapshot()));
    showMessage(canvas_->hasSelection() ? QStringLiteral("Selection cleared") : QStringLiteral("Layer cleared"));
}

void MainWindow::copyActiveLayer()
{
    if (!canvas_->hasDocument()) {
        return;
    }
    const Layer *layer = canvas_->document().activeLayer();
    if (!layer || layer->image.isNull()) {
        return;
    }

    const QRect sourceRect = canvas_->hasSelection()
        ? canvas_->selectionRect().intersected(QRect(QPoint(0, 0), layer->image.size()))
        : QRect(QPoint(0, 0), layer->image.size());
    if (!sourceRect.isValid()) {
        showMessage(QStringLiteral("Nothing to copy"));
        return;
    }
    QGuiApplication::clipboard()->setImage(layer->image.copy(sourceRect).convertToFormat(QImage::Format_ARGB32));
    showMessage(QStringLiteral("Copied %1 x %2").arg(sourceRect.width()).arg(sourceRect.height()));
}

void MainWindow::cutActiveLayer()
{
    if (!canvas_->hasDocument()) {
        return;
    }
    const Layer *layer = canvas_->document().activeLayer();
    if (!layer) {
        return;
    }
    if (layer->locked) {
        showMessage(QStringLiteral("Layer is locked"));
        return;
    }

    copyActiveLayer();
    const QRect target = canvas_->hasSelection() ? canvas_->selectionRect() : QRect();
    const DocumentState before = canvas_->document().snapshot();
    if (!canvas_->document().clearLayer(canvas_->document().activeLayerIndex(), target)) {
        return;
    }
    canvas_->commandManager()->execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Cut"), &canvas_->document(), before, canvas_->document().snapshot()));
    showMessage(QStringLiteral("Cut to clipboard"));
}

void MainWindow::pasteImageFromClipboard()
{
    if (!canvas_->hasDocument()) {
        return;
    }
    const QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull()) {
        showMessage(QStringLiteral("Clipboard has no image"));
        return;
    }

    const DocumentState before = canvas_->document().snapshot();
    QString error;
    if (!canvas_->document().addImageLayer(image, QStringLiteral("Pasted Image"), &error, true)) {
        QMessageBox::warning(this, QStringLiteral("Paste"), error);
        return;
    }
    canvas_->commandManager()->execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Paste Image"), &canvas_->document(), before, canvas_->document().snapshot()));
    showMessage(QStringLiteral("Pasted image as new layer"));
}

void MainWindow::showPreferences()
{
    QMessageBox::information(
        this,
        QStringLiteral("Preferences"),
        QStringLiteral("HXPainter preferences are stored with the runtime.\n\nExecutable directory:\n%1\n\nDiagnostics log:\n%2")
            .arg(AppPaths::executableDir(), AppPaths::diagnosticsLogPath()));
}

void MainWindow::showFilterGallery()
{
    if (!canvas_->hasDocument()) {
        return;
    }
    Layer *layer = canvas_->document().activeLayer();
    if (!layer) {
        return;
    }
    if (layer->locked) {
        showMessage(QStringLiteral("Layer is locked"));
        return;
    }

    bool ok = false;
    const QString filter = QInputDialog::getItem(
        this,
        QStringLiteral("Filter Gallery"),
        QStringLiteral("Filter"),
        {QStringLiteral("Invert Colors"), QStringLiteral("Grayscale")},
        0,
        false,
        &ok);
    if (!ok || filter.isEmpty()) {
        return;
    }

    const QRect target = (canvas_->hasSelection() ? canvas_->selectionRect() : QRect(QPoint(0, 0), layer->image.size()))
        .intersected(QRect(QPoint(0, 0), layer->image.size()));
    if (!target.isValid()) {
        showMessage(QStringLiteral("Nothing to filter"));
        return;
    }

    const DocumentState before = canvas_->document().snapshot();
    QImage working = layer->image.convertToFormat(QImage::Format_ARGB32);
    for (int y = target.top(); y <= target.bottom(); ++y) {
        for (int x = target.left(); x <= target.right(); ++x) {
            QColor color = QColor::fromRgba(working.pixel(x, y));
            if (filter == QStringLiteral("Invert Colors")) {
                color.setRgb(255 - color.red(), 255 - color.green(), 255 - color.blue(), color.alpha());
            } else {
                const int gray = qGray(color.rgb());
                color.setRgb(gray, gray, gray, color.alpha());
            }
            working.setPixelColor(x, y, color);
        }
    }
    layer->image = working.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    canvas_->document().setModified(true);
    canvas_->commandManager()->execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Filter: %1").arg(filter), &canvas_->document(), before, canvas_->document().snapshot()));
    showMessage(QStringLiteral("Applied %1").arg(filter));
}

void MainWindow::refreshUiState()
{
    const bool hasDocument = canvas_->hasDocument();
    actions_->setDocumentAvailable(hasDocument);
    actions_->setUndoRedoAvailable(canvas_->commandManager()->canUndo(), canvas_->commandManager()->canRedo());
    refreshLayers();
    refreshHistory();
    refreshNavigator();
    refreshStatus();
}

void MainWindow::refreshLayers()
{
    QSignalBlocker block(layersList_);
    layersList_->clear();
    const QVector<Layer> &layers = canvas_->document().layers();
    for (int i = layers.size() - 1; i >= 0; --i) {
        const Layer &layer = layers[i];
        QListWidgetItem *item = new QListWidgetItem(
            QStringLiteral("%1 %2%3%4")
                .arg(i == canvas_->document().activeLayerIndex() ? ">" : " ")
                .arg(layer.visible ? "[V]" : "[H]")
                .arg(layer.locked ? " [L] " : (layer.alphaLock ? " [A] " : " "))
                .arg(layer.name));
        item->setData(Qt::UserRole, i);
        item->setIcon(QPixmap::fromImage(layer.thumbnail()));
        layersList_->addItem(item);
        if (i == canvas_->document().activeLayerIndex()) {
            layersList_->setCurrentItem(item);
            QSignalBlocker opacityBlock(layerOpacitySlider_);
            layerOpacitySlider_->setValue(static_cast<int>(layer.opacity * 100.0));
            QSignalBlocker blendBlock(blendModeCombo_);
            blendModeCombo_->setCurrentText(layer.blendMode);
        }
    }
}

void MainWindow::refreshHistory()
{
    historyList_->clear();
    historyList_->addItems(canvas_->commandManager()->undoHistory());
    if (historyList_->count() > 0) {
        historyList_->setCurrentRow(historyList_->count() - 1);
    }
}

void MainWindow::refreshNavigator()
{
    if (!canvas_->hasDocument()) {
        navigatorLabel_->setText(QStringLiteral("No document"));
        return;
    }
    navigatorLabel_->setPixmap(QPixmap::fromImage(canvas_->document().compositedImage()).scaled(
        navigatorLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::refreshStatus()
{
    const StatsSnapshot stats = canvas_->statsSnapshot();
    if (canvas_->hasDocument()) {
        docSizeStatus_->setText(QStringLiteral("%1 x %2").arg(canvas_->document().size().width()).arg(canvas_->document().size().height()));
    } else {
        docSizeStatus_->setText(QStringLiteral("No document"));
    }
    zoomStatus_->setText(QStringLiteral("%1%").arg(canvas_->zoom() * 100.0, 0, 'f', 0));
    toolStatus_->setText(toolTypeName(toolManager_->activeToolType()));
    const Layer *layer = canvas_->document().activeLayer();
    layerStatus_->setText(layer ? layer->name : QStringLiteral("No layer"));
    brushStatus_->setText(QStringLiteral("Brush %1px").arg(canvas_->brushSettings().radius, 0, 'f', 0));
    colorStatus_->setText(canvas_->foregroundColor().name(QColor::HexArgb));
    layerOpacityStatus_->setText(layer ? QStringLiteral("Layer %1%").arg(layer->opacity * 100.0, 0, 'f', 0) : QStringLiteral("Layer --"));
    fpsStatus_->setText(QStringLiteral("FPS %1").arg(stats.fps, 0, 'f', 1));
}

void MainWindow::refreshDiagnostics()
{
    const StatsSnapshot stats = canvas_->statsSnapshot();
    QString text = QString(
        "FPS: %1\nFrame: %2 ms\nStroke: %3 ms\nInput: %4 Hz\nPressure: %5\nInput Events: %6\nStroke Samples: %7")
        .arg(stats.fps, 0, 'f', 1)
        .arg(stats.frameMs, 0, 'f', 2)
        .arg(stats.strokeMs, 0, 'f', 2)
        .arg(stats.inputHz, 0, 'f', 1)
        .arg(stats.lastPressure, 0, 'f', 2)
        .arg(stats.inputEvents)
        .arg(stats.strokeSamples);
    if (!startupDiagnostics_.isEmpty()) {
        text += QStringLiteral("\n\nStartup Log:\n") + startupDiagnostics_.join(QStringLiteral("\n"));
    }
    diagnosticsLabel_->setText(text);
}

void MainWindow::updateColorControls(const QColor &color)
{
    QSignalBlocker r(redSpin_);
    QSignalBlocker g(greenSpin_);
    QSignalBlocker b(blueSpin_);
    QSignalBlocker a(alphaSpin_);
    redSpin_->setValue(color.red());
    greenSpin_->setValue(color.green());
    blueSpin_->setValue(color.blue());
    alphaSpin_->setValue(color.alpha());
    foregroundButton_->setStyleSheet(swatchStyle(color));
    refreshStatus();
}

void MainWindow::setForegroundFromControls()
{
    if (updatingColorControls_) {
        return;
    }
    QColor color(redSpin_->value(), greenSpin_->value(), blueSpin_->value(), alphaSpin_->value());
    canvas_->setForegroundColor(color);
    foregroundButton_->setStyleSheet(swatchStyle(color));
    refreshStatus();
}

void MainWindow::chooseForegroundColor()
{
    const QColor selected = QColorDialog::getColor(canvas_->foregroundColor(), this, QStringLiteral("Foreground Color"),
                                                   QColorDialog::ShowAlphaChannel);
    if (selected.isValid()) {
        canvas_->setForegroundColor(selected);
        updateColorControls(selected);
    }
}

void MainWindow::chooseBackgroundColor()
{
    const QColor selected = QColorDialog::getColor(canvas_->backgroundColor(), this, QStringLiteral("Background Color"),
                                                   QColorDialog::ShowAlphaChannel);
    if (selected.isValid()) {
        canvas_->setBackgroundColor(selected);
        backgroundButton_->setStyleSheet(swatchStyle(selected));
    }
}

void MainWindow::applyBrushPreset(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    BrushSettings settings = canvas_->brushSettings();
    const QString name = item->text();
    if (name == QStringLiteral("Pencil")) {
        settings.radius = 3; settings.opacity = 1.0; settings.flow = 1.0; settings.hardness = 1.0;
    } else if (name == QStringLiteral("Ink")) {
        settings.radius = 8; settings.opacity = 1.0; settings.flow = 0.95; settings.hardness = 0.85;
    } else if (name == QStringLiteral("Hard Brush")) {
        settings.radius = 18; settings.opacity = 1.0; settings.flow = 1.0; settings.hardness = 1.0;
    } else if (name == QStringLiteral("Soft Brush")) {
        settings.radius = 32; settings.opacity = 0.65; settings.flow = 0.65; settings.hardness = 0.25;
    } else if (name == QStringLiteral("Marker")) {
        settings.radius = 24; settings.opacity = 0.45; settings.flow = 0.75; settings.hardness = 0.65;
    } else if (name == QStringLiteral("Airbrush")) {
        settings.radius = 48; settings.opacity = 0.25; settings.flow = 0.35; settings.hardness = 0.05;
    }
    canvas_->setBrushSettings(settings);
    toolOptionsPanel_->setBrushSettings(settings);
    showMessage(QStringLiteral("Brush preset: %1").arg(name));
}

void MainWindow::showLayerOpacityDialog()
{
    const Layer *layer = canvas_->document().activeLayer();
    if (!layer) {
        return;
    }
    bool ok = false;
    const int value = QInputDialog::getInt(this, QStringLiteral("Layer Opacity"), QStringLiteral("Opacity"),
                                           static_cast<int>(layer->opacity * 100.0), 0, 100, 1, &ok);
    if (ok) {
        canvas_->layerManager()->setActiveLayerOpacity(value / 100.0);
    }
    canvas_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::renameActiveLayer()
{
    const Layer *layer = canvas_->document().activeLayer();
    if (!layer) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Rename Layer"), QStringLiteral("Name"),
                                               QLineEdit::Normal, layer->name, &ok);
    if (ok) {
        canvas_->layerManager()->renameActiveLayer(name);
    }
    canvas_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::toggleActiveLayerVisibility()
{
    const Layer *layer = canvas_->document().activeLayer();
    if (layer) {
        canvas_->layerManager()->setActiveLayerVisible(!layer->visible);
    }
}

void MainWindow::toggleActiveLayerLock()
{
    const Layer *layer = canvas_->document().activeLayer();
    if (layer) {
        canvas_->layerManager()->setActiveLayerLocked(!layer->locked);
    }
}

void MainWindow::toggleActiveLayerAlphaLock()
{
    const Layer *layer = canvas_->document().activeLayer();
    if (layer) {
        canvas_->layerManager()->setActiveLayerAlphaLock(!layer->alphaLock);
    }
}

void MainWindow::showBlendModeDialog()
{
    const Layer *layer = canvas_->document().activeLayer();
    if (!layer) {
        return;
    }
    bool ok = false;
    const QStringList modes = {QStringLiteral("Normal"), QStringLiteral("Multiply"), QStringLiteral("Screen"), QStringLiteral("Overlay")};
    const int currentMode = std::max(0, static_cast<int>(modes.indexOf(layer->blendMode)));
    const QString mode = QInputDialog::getItem(this, QStringLiteral("Blend Mode"), QStringLiteral("Mode"),
                                               modes, currentMode, false, &ok);
    if (ok) {
        canvas_->layerManager()->setActiveLayerBlendMode(mode);
    }
}

void MainWindow::showMessage(const QString &message)
{
    statusBar()->showMessage(message, 4000);
}

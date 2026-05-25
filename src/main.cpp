#include "actions/ActionRegistry.h"
#include "app/AppPaths.h"
#include "app/IconLoader.h"
#include "brush/BrushEngine.h"
#include "brush/EraserEngine.h"
#include "brush/FillEngine.h"
#include "brush/TextEngine.h"
#include "commands/Command.h"
#include "commands/CommandManager.h"
#include "export/ExportManager.h"
#include "input/TabletInputMapper.h"
#include "serialization/ProjectSerializer.h"
#include "ui/AppTheme.h"
#include "ui/CreateOpenHub.h"
#include "ui/MainWindow.h"
#include "ui/panels/ToolOptionsPanel.h"

#include <QApplication>
#include <QAction>
#include <QDateTime>
#include <QDialog>
#include <QListWidget>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
#include <QMouseEvent>
#include <QPainter>
#include <QPointingDevice>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTouchEvent>
#include <QDebug>

#include <cmath>
#include <memory>

static int runMvpSmokeTest()
{
    auto fail = [](const QString &message) {
        qWarning().noquote() << "MVP smoke test failed:" << message;
        return 20;
    };

    if (!QFileInfo::exists(AppPaths::logoPath())) {
        return fail(QStringLiteral("logo.png not found beside executable: %1").arg(AppPaths::logoPath()));
    }

    CanvasDocument document;
    NewDocumentSettings settings;
    settings.width = 160;
    settings.height = 120;
    settings.backgroundColor = Qt::white;
    settings.defaultLayerCount = 1;
    settings.templateName = QStringLiteral("Illustration");
    document.reset(settings);
    if (!document.isValid() || document.size() != QSize(160, 120)) {
        return fail(QStringLiteral("new document creation failed"));
    }

    CommandManager commands;
    BrushEngine brush;
    EraserEngine eraser;

    BrushSettings blackBrush;
    blackBrush.color = QColor(0, 0, 0, 255);
    blackBrush.radius = 8;
    blackBrush.opacity = 1.0;
    DocumentState beforeBrush = document.snapshot();
    brush.beginStroke(*document.activeImage(), StrokeSample{QPointF(30, 60), 1.0, 1}, blackBrush);
    brush.continueStroke(*document.activeImage(), StrokeSample{QPointF(130, 60), 1.0, 2}, blackBrush);
    brush.endStroke();
    commands.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Brush Stroke"), &document, beforeBrush, document.snapshot()));
    const QImage layerOneAfterBrush = document.layers()[0].image;
    commands.undo();
    if (document.layers()[0].image != beforeBrush.layers[0].image) {
        return fail(QStringLiteral("first undo did not restore the initial blank document state"));
    }
    commands.redo();
    if (document.layers()[0].image != layerOneAfterBrush) {
        return fail(QStringLiteral("redo after first undo did not restore the first brush stroke"));
    }

    DocumentState beforeAdd = document.snapshot();
    document.addLayer(QStringLiteral("Layer 2"));
    commands.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Add Layer"), &document, beforeAdd, document.snapshot()));
    if (document.layers().size() != 2 || document.activeLayerIndex() != 1) {
        return fail(QStringLiteral("layer add failed"));
    }

    BrushSettings redBrush;
    redBrush.color = QColor(220, 0, 0, 255);
    redBrush.radius = 10;
    redBrush.opacity = 1.0;
    DocumentState beforeRed = document.snapshot();
    brush.beginStroke(*document.activeImage(), StrokeSample{QPointF(30, 60), 1.0, 3}, redBrush);
    brush.continueStroke(*document.activeImage(), StrokeSample{QPointF(130, 60), 1.0, 4}, redBrush);
    brush.endStroke();
    commands.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Brush Stroke"), &document, beforeRed, document.snapshot()));
    const QRgb redBeforeErase = document.layers()[1].image.pixel(80, 60);

    EraserSettings eraserSettings;
    eraserSettings.radius = 16;
    eraserSettings.opacity = 1.0;
    DocumentState beforeErase = document.snapshot();
    eraser.beginStroke(*document.activeImage(), StrokeSample{QPointF(80, 60), 1.0, 5}, eraserSettings);
    eraser.endStroke();
    commands.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Erase Stroke"), &document, beforeErase, document.snapshot()));
    if (document.layers()[0].image != layerOneAfterBrush) {
        return fail(QStringLiteral("eraser changed a non-active layer"));
    }
    const int erasedAlpha = qAlpha(document.layers()[1].image.pixel(80, 60));
    if (erasedAlpha >= qAlpha(redBeforeErase)) {
        return fail(QStringLiteral("eraser did not reduce active layer alpha"));
    }

    commands.undo();
    if (qAlpha(document.layers()[1].image.pixel(80, 60)) <= erasedAlpha) {
        return fail(QStringLiteral("undo did not restore erase stroke"));
    }
    commands.redo();
    if (qAlpha(document.layers()[1].image.pixel(80, 60)) != erasedAlpha) {
        return fail(QStringLiteral("redo did not reapply erase stroke"));
    }

    document.setActiveLayerIndex(0);
    FillSettings fillSettings;
    fillSettings.tolerance = 0;
    fillSettings.contiguous = true;
    DocumentState beforeFill = document.snapshot();
    DocumentState afterFill = beforeFill;
    FillResult fillResult = FillEngine::floodFill(afterFill.layers[afterFill.activeLayerIndex].image,
                                                  QPoint(2, 2),
                                                  QColor(0, 180, 80, 180),
                                                  fillSettings);
    if (!fillResult.hasChanges()) {
        return fail(QStringLiteral("fill did not change pixels"));
    }
    afterFill.modified = true;
    commands.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Fill"), &document, beforeFill, afterFill));
    const QRgb filledPixel = document.layers()[0].image.pixel(2, 2);
    if (filledPixel == beforeFill.layers[0].image.pixel(2, 2)) {
        return fail(QStringLiteral("fill command did not apply"));
    }
    commands.undo();
    if (document.layers()[0].image.pixel(2, 2) != beforeFill.layers[0].image.pixel(2, 2)) {
        return fail(QStringLiteral("undo did not remove fill"));
    }
    commands.redo();
    if (document.layers()[0].image.pixel(2, 2) != filledPixel) {
        return fail(QStringLiteral("redo did not restore fill"));
    }

    DocumentState beforeText = document.snapshot();
    TextSettings textSettings;
    textSettings.font = QApplication::font();
    textSettings.font.setPointSize(18);
    textSettings.color = QColor(20, 20, 20, 255);
    const TextRenderResult textResult = TextEngine::drawTextBox(*document.activeImage(), QPointF(10, 20), QStringLiteral("HXPainter"), textSettings);
    if (!textResult.changed) {
        return fail(QStringLiteral("text box insertion failed"));
    }
    commands.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("Text"), &document, beforeText, document.snapshot()));

    QTemporaryDir temp;
    if (!temp.isValid()) {
        return fail(QStringLiteral("could not create temp directory"));
    }
    const QString projectPath = temp.filePath(QStringLiteral("smoke.hxp"));
    QString error;
    if (!ProjectSerializer::save(document, projectPath, &error)) {
        return fail(QStringLiteral("save failed: %1").arg(error));
    }
    CanvasDocument loaded;
    if (!ProjectSerializer::load(loaded, projectPath, &error)) {
        return fail(QStringLiteral("load failed: %1").arg(error));
    }
    if (loaded.layers().size() != document.layers().size()) {
        return fail(QStringLiteral("loaded layer count mismatch"));
    }

    QFile malformedProject(temp.filePath(QStringLiteral("malformed.hxp")));
    if (!malformedProject.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return fail(QStringLiteral("could not create malformed project file"));
    }
    malformedProject.write("{");
    malformedProject.close();
    CanvasDocument malformedLoaded;
    if (ProjectSerializer::load(malformedLoaded, malformedProject.fileName(), &error)
        || !error.contains(QStringLiteral("Invalid HXP project JSON"))) {
        return fail(QStringLiteral("malformed project did not return a parse error"));
    }

    QImage importImage(24, 24, QImage::Format_ARGB32_Premultiplied);
    importImage.fill(QColor(0, 0, 255, 180));
    const QString importPath = temp.filePath(QStringLiteral("import.png"));
    importImage.save(importPath);
    const int beforeImportCount = loaded.layers().size();
    if (!loaded.importImageAsLayer(importPath, &error) || loaded.layers().size() != beforeImportCount + 1) {
        return fail(QStringLiteral("import as new layer failed: %1").arg(error));
    }

    ExportOptions pngOptions;
    pngOptions.filePath = temp.filePath(QStringLiteral("export.png"));
    pngOptions.preserveTransparency = true;
    if (!ExportManager::exportDocument(loaded, pngOptions, &error) || !QFileInfo::exists(pngOptions.filePath)) {
        return fail(QStringLiteral("PNG export failed: %1").arg(error));
    }
    ExportOptions jpgOptions;
    jpgOptions.filePath = temp.filePath(QStringLiteral("export.jpg"));
    jpgOptions.preserveTransparency = false;
    if (!ExportManager::exportDocument(loaded, jpgOptions, &error) || !QFileInfo::exists(jpgOptions.filePath)) {
        return fail(QStringLiteral("JPG export failed: %1").arg(error));
    }

    QFile logFile(AppPaths::diagnosticsLogPath());
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString(Qt::ISODate)
               << " MVP smoke test passed: new document, brush, add layer, eraser active-layer alpha, undo, redo, fill, text box, hxp save/load, malformed load diagnostics, import layer, PNG/JPG export, executable-side logo"
               << Qt::endl;
    }

    qInfo().noquote() << "MVP smoke test passed";
    qInfo().noquote() << "Checked: new document, brush, add layer, eraser active-layer alpha, undo, redo, fill, text box, hxp save/load, malformed load diagnostics, import layer, PNG/JPG export, executable-side logo";
    return 0;
}

static int runThemeSmokeTest()
{
    auto fail = [](const QString &message, int code) {
        QFile logFile(AppPaths::diagnosticsLogPath());
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&logFile);
            stream << QDateTime::currentDateTime().toString(Qt::ISODate)
                   << " Theme smoke test failed: " << message << Qt::endl;
        }
        qWarning().noquote() << "Theme smoke test failed:" << message;
        return code;
    };

    QFile themeFile(QStringLiteral(":/theme/hxpainter_dark.qss"));
    if (!themeFile.exists() || qApp->styleSheet().isEmpty()) {
        return fail(QStringLiteral("theme stylesheet was not loaded"), 31);
    }

    const QStringList requiredIcons = {
        QStringLiteral(":/icons/brush.svg"),
        QStringLiteral(":/icons/eraser.svg"),
        QStringLiteral(":/icons/bucket.svg"),
        QStringLiteral(":/icons/color-picker.svg"),
        QStringLiteral(":/icons/file-plus.svg"),
        QStringLiteral(":/icons/device-floppy.svg")
    };
    for (const QString &iconPath : requiredIcons) {
        if (QIcon(iconPath).isNull()) {
            return fail(QStringLiteral("icon resource did not load: %1").arg(iconPath), 32);
        }
    }

    ActionRegistry actions;
    if (!actions.rotateLeft || actions.rotateLeft->shortcut() != QKeySequence(QStringLiteral("Ctrl+["))) {
        return fail(QStringLiteral("rotate-left action or shortcut is missing"), 35);
    }
    if (!actions.rotateRight || actions.rotateRight->shortcut() != QKeySequence(QStringLiteral("Ctrl+]"))) {
        return fail(QStringLiteral("rotate-right action or shortcut is missing"), 36);
    }
    if (!actions.transform || actions.transform->shortcut() != QKeySequence(QStringLiteral("Ctrl+T"))) {
        return fail(QStringLiteral("transform action or shortcut is missing"), 37);
    }
    for (QAction *action : actions.sideToolActions()) {
        const ToolType type = static_cast<ToolType>(action->data().toInt());
        if (type == ToolType::Move || type == ToolType::Hand || type == ToolType::Zoom) {
            return fail(QStringLiteral("left sidebar contains duplicate navigation tool: %1").arg(action->text()), 33);
        }
        if (action->icon().isNull()) {
            return fail(QStringLiteral("side tool has no icon: %1").arg(action->text()), 34);
        }
    }

    qInfo().noquote() << "Theme smoke test passed";
    qInfo().noquote() << "Checked: dark stylesheet loaded, required SVG icons loaded, rotate-left/right shortcuts, Transform shortcut, left sidebar excludes Move/Hand/Zoom";
    return 0;
}

static int runFunctionalRegressionSmokeTest()
{
    auto fail = [](const QString &message) {
        QFile logFile(AppPaths::diagnosticsLogPath());
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&logFile);
            stream << QDateTime::currentDateTime().toString(Qt::ISODate)
                   << " Functional regression smoke test failed: " << message << Qt::endl;
        }
        qWarning().noquote() << "Functional regression smoke test failed:" << message;
        return 40;
    };

    QTemporaryDir temp;
    if (!temp.isValid()) {
        return fail(QStringLiteral("could not create temp directory"));
    }

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings().clear();

    CanvasDocument savedRecentDoc;
    NewDocumentSettings savedRecentDocSettings;
    savedRecentDocSettings.width = 24;
    savedRecentDocSettings.height = 18;
    savedRecentDocSettings.defaultLayerCount = 1;
    savedRecentDoc.reset(savedRecentDocSettings);
    const QString savedRecentProjectPath = temp.filePath(QStringLiteral("saved-recent-project.hxp"));
    QString recentError;
    if (!ProjectSerializer::save(savedRecentDoc, savedRecentProjectPath, &recentError)) {
        return fail(QStringLiteral("could not create saved recent project fixture: %1").arg(recentError));
    }

    CanvasDocument loadedRecentDoc;
    NewDocumentSettings loadedRecentDocSettings;
    loadedRecentDocSettings.width = 32;
    loadedRecentDocSettings.height = 20;
    loadedRecentDocSettings.defaultLayerCount = 1;
    loadedRecentDoc.reset(loadedRecentDocSettings);
    const QString loadedRecentProjectPath = temp.filePath(QStringLiteral("loaded-recent-project.hxp"));
    if (!ProjectSerializer::save(loadedRecentDoc, loadedRecentProjectPath, &recentError)) {
        return fail(QStringLiteral("could not create loaded recent project fixture: %1").arg(recentError));
    }

    CreateOpenHub::rememberSavedProject(savedRecentProjectPath);
    CreateOpenHub::rememberLoadedProject(loadedRecentProjectPath);
    const QStringList recentPaths = CreateOpenHub::recentProjectPaths();
    if (recentPaths.size() != 2
        || QFileInfo(recentPaths.at(0)).canonicalFilePath() != QFileInfo(loadedRecentProjectPath).canonicalFilePath()
        || QFileInfo(recentPaths.at(1)).canonicalFilePath() != QFileInfo(savedRecentProjectPath).canonicalFilePath()) {
        return fail(QStringLiteral("saved and loaded project history was not persisted in recent order"));
    }

    auto findButton = [](QWidget *root, const QString &text) -> QPushButton * {
        const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
        for (QPushButton *button : buttons) {
            if (button->text() == text) {
                return button;
            }
        }
        return nullptr;
    };

    CreateOpenHub recentHub;
    QListWidget *recentList = recentHub.findChild<QListWidget *>(QStringLiteral("recentProjectsList"));
    QTabWidget *recentTabs = recentHub.findChild<QTabWidget *>();
    QPushButton *openButton = findButton(&recentHub, QStringLiteral("Open"));
    if (!recentList || recentList->count() != 2 || !recentTabs || !openButton) {
        return fail(QStringLiteral("recent projects UI was not populated"));
    }
    recentList->setCurrentRow(0);
    recentTabs->setCurrentWidget(recentList);
    openButton->click();
    if (recentHub.mode() != CreateOpenHub::Mode::OpenExisting
        || QFileInfo(recentHub.selectedFilePath()).canonicalFilePath() != QFileInfo(loadedRecentProjectPath).canonicalFilePath()) {
        return fail(QStringLiteral("recent project selection did not open the selected path"));
    }

    CreateOpenHub templateHub;
    QListWidget *templatesList = templateHub.findChild<QListWidget *>(QStringLiteral("templatesList"));
    QTabWidget *templateTabs = templateHub.findChild<QTabWidget *>();
    QPushButton *createButton = findButton(&templateHub, QStringLiteral("Create"));
    if (!templatesList || templatesList->count() < 3 || !templateTabs || !createButton) {
        return fail(QStringLiteral("template UI was not populated"));
    }
    templatesList->setCurrentRow(2);
    templateTabs->setCurrentWidget(templatesList);
    createButton->click();
    const NewDocumentSettings webtoonSettings = templateHub.newDocumentSettings();
    if (templateHub.mode() != CreateOpenHub::Mode::NewDocument
        || webtoonSettings.templateName != QStringLiteral("Webtoon")
        || webtoonSettings.width != 1600
        || webtoonSettings.height != 6000
        || webtoonSettings.defaultLayerCount != 4) {
        return fail(QStringLiteral("template tab selection was not applied to new document settings"));
    }

    QMouseEvent fillPress(QEvent::MouseButtonPress,
                          QPointF(4, 5),
                          QPointF(4, 5),
                          QPointF(4, 5),
                          Qt::LeftButton,
                          Qt::NoButton,
                          Qt::NoModifier);
    const TabletInputEvent mappedFillPress = TabletInputMapper::fromMouseEvent(fillPress, QPointF(4, 5));
    if (!mappedFillPress.isPrimaryDown() || mappedFillPress.pressure <= 0.0) {
        return fail(QStringLiteral("mouse press mapper did not preserve primary button state"));
    }

    CanvasDocument noOpDoc;
    NewDocumentSettings noOpSettings;
    noOpSettings.width = 8;
    noOpSettings.height = 8;
    noOpSettings.transparentBackground = true;
    noOpSettings.defaultLayerCount = 1;
    noOpDoc.reset(noOpSettings);
    CommandManager noOpCommands;
    DocumentState noOpBefore = noOpDoc.snapshot();
    DocumentState noOpAfter = noOpBefore;
    noOpAfter.modified = true;
    noOpCommands.execute(std::make_unique<DocumentSnapshotCommand>(
        QStringLiteral("No-op"), &noOpDoc, noOpBefore, noOpAfter));
    if (noOpCommands.canUndo()) {
        return fail(QStringLiteral("no-op document command was recorded in undo history"));
    }

    QImage transparentStroke(24, 24, QImage::Format_ARGB32_Premultiplied);
    transparentStroke.fill(Qt::transparent);
    BrushSettings transparentBrush;
    transparentBrush.color = QColor(0, 0, 0, 0);
    BrushEngine transparentBrushEngine;
    if (transparentBrushEngine.beginStroke(transparentStroke, StrokeSample{QPointF(12, 12), 1.0, 1}, transparentBrush).hasChanges()) {
        return fail(QStringLiteral("transparent brush stamp was treated as a pixel change"));
    }
    EraserEngine emptyEraserEngine;
    EraserSettings emptyEraser;
    if (emptyEraserEngine.beginStroke(transparentStroke, StrokeSample{QPointF(12, 12), 1.0, 2}, emptyEraser).hasChanges()) {
        return fail(QStringLiteral("erasing transparent pixels was treated as a pixel change"));
    }

    ToolOptionsPanel brushOptions;
    int brushSettingsEmissionCount = 0;
    BrushSettings lastBrushSettings;
    QObject::connect(&brushOptions, &ToolOptionsPanel::brushSettingsChanged, &brushOptions, [&](const BrushSettings &settings) {
        ++brushSettingsEmissionCount;
        lastBrushSettings = settings;
    });
    BrushSettings configuredBrush;
    configuredBrush.color = QColor(10, 20, 30, 77);
    configuredBrush.radius = 18;
    configuredBrush.opacity = 0.82;
    configuredBrush.flow = 0.73;
    configuredBrush.hardness = 0.42;
    configuredBrush.spacing = 0.35;
    configuredBrush.pressureControlsRadius = false;
    configuredBrush.pressureControlsOpacity = true;
    brushOptions.setBrushSettings(configuredBrush);
    if (brushSettingsEmissionCount != 0) {
        return fail(QStringLiteral("programmatic brush settings update emitted partial settings"));
    }
    QSpinBox *brushSizeControl = brushOptions.findChild<QSpinBox *>(QStringLiteral("BrushSize"));
    if (!brushSizeControl) {
        return fail(QStringLiteral("brush size control was not discoverable for regression test"));
    }
    brushSizeControl->setValue(41);
    const auto closeEnough = [](double lhs, double rhs) {
        return std::abs(lhs - rhs) <= 0.000001;
    };
    if (brushSettingsEmissionCount != 1
        || !closeEnough(lastBrushSettings.radius, 41.0)
        || !closeEnough(lastBrushSettings.opacity, configuredBrush.opacity)
        || !closeEnough(lastBrushSettings.flow, configuredBrush.flow)
        || !closeEnough(lastBrushSettings.hardness, configuredBrush.hardness)
        || !closeEnough(lastBrushSettings.spacing, configuredBrush.spacing)
        || lastBrushSettings.color.alpha() != configuredBrush.color.alpha()
        || lastBrushSettings.pressureControlsRadius != configuredBrush.pressureControlsRadius
        || lastBrushSettings.pressureControlsOpacity != configuredBrush.pressureControlsOpacity) {
        return fail(QStringLiteral("brush size change did not preserve opacity, flow, alpha, or pressure settings"));
    }

    OpenGLCanvasWidget strokeCanvas;
    strokeCanvas.resize(32, 32);
    NewDocumentSettings strokeSettings;
    strokeSettings.width = 32;
    strokeSettings.height = 32;
    strokeSettings.transparentBackground = true;
    strokeSettings.defaultLayerCount = 1;
    strokeCanvas.newDocument(strokeSettings);
    strokeCanvas.setForegroundColor(QColor(0, 0, 0, 255));
    BrushSettings strokeBrush;
    strokeBrush.radius = 3;
    strokeBrush.opacity = 1.0;
    strokeCanvas.setBrushSettings(strokeBrush);
    const QImage beforeWidgetStroke = strokeCanvas.document().layers()[0].image;
    auto nonTransparentPixelCount = [](const QImage &image) {
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 0) {
                    ++count;
                }
            }
        }
        return count;
    };

    QMouseEvent strokePress(QEvent::MouseButtonPress,
                            QPointF(4, 16),
                            QPointF(4, 16),
                            QPointF(4, 16),
                            Qt::LeftButton,
                            Qt::NoButton,
                            Qt::NoModifier);
    QApplication::sendEvent(&strokeCanvas, &strokePress);
    QMouseEvent duplicateStrokePress(QEvent::MouseButtonPress,
                                     QPointF(4, 16),
                                     QPointF(4, 16),
                                     QPointF(4, 16),
                                     Qt::LeftButton,
                                     Qt::NoButton,
                                     Qt::NoModifier);
    QApplication::sendEvent(&strokeCanvas, &duplicateStrokePress);
    if (!strokeCanvas.commandManager()->undoHistory().isEmpty()) {
        return fail(QStringLiteral("duplicate brush press committed an early undo command"));
    }
    QMouseEvent strokeMove(QEvent::MouseMove,
                           QPointF(24, 16),
                           QPointF(24, 16),
                           QPointF(24, 16),
                           Qt::NoButton,
                           Qt::LeftButton,
                           Qt::NoModifier);
    QApplication::sendEvent(&strokeCanvas, &strokeMove);
    QMouseEvent strokeRelease(QEvent::MouseButtonRelease,
                              QPointF(24, 16),
                              QPointF(24, 16),
                              QPointF(24, 16),
                              Qt::LeftButton,
                              Qt::NoButton,
                              Qt::NoModifier);
    QApplication::sendEvent(&strokeCanvas, &strokeRelease);
    if (strokeCanvas.commandManager()->undoHistory().size() != 1) {
        return fail(QStringLiteral("duplicate brush press split one stroke into multiple undo commands"));
    }
    if (strokeCanvas.document().layers()[0].image == beforeWidgetStroke) {
        return fail(QStringLiteral("widget brush stroke did not change pixels"));
    }
    strokeCanvas.commandManager()->undo();
    if (strokeCanvas.document().layers()[0].image != beforeWidgetStroke) {
        return fail(QStringLiteral("one undo did not remove the full widget brush stroke; before=%1 afterUndo=%2")
            .arg(nonTransparentPixelCount(beforeWidgetStroke))
            .arg(nonTransparentPixelCount(strokeCanvas.document().layers()[0].image)));
    }

    MainWindow tabletFocusWindow;
    tabletFocusWindow.resize(900, 600);
    OpenGLCanvasWidget *tabletFocusCanvas = tabletFocusWindow.findChild<OpenGLCanvasWidget *>();
    QSpinBox *tabletFocusBrushSize = tabletFocusWindow.findChild<QSpinBox *>(QStringLiteral("BrushSize"));
    if (!tabletFocusCanvas || !tabletFocusBrushSize) {
        return fail(QStringLiteral("tablet focus regression test could not find canvas or brush size control"));
    }
    const auto resetTabletFocusCanvas = [&] {
        tabletFocusCanvas->newDocument(strokeSettings);
        tabletFocusCanvas->setForegroundColor(QColor(0, 0, 0, 255));
        tabletFocusCanvas->setBrushSettings(strokeBrush);
        tabletFocusCanvas->fitCanvas();
        QApplication::processEvents();
    };
    resetTabletFocusCanvas();
    tabletFocusWindow.show();
    tabletFocusWindow.activateWindow();
    QApplication::processEvents();
    tabletFocusCanvas->fitCanvas();
    QApplication::processEvents();

    tabletFocusBrushSize->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const QImage beforeTabletFocusStroke = tabletFocusCanvas->document().layers()[0].image;
    const QPointF tabletCanvasPosition = QPointF(tabletFocusCanvas->rect().center());
    const QPointF tabletGlobalPosition = tabletFocusCanvas->mapToGlobal(tabletCanvasPosition);
    const QPointF tabletTargetPosition = tabletFocusBrushSize->mapFromGlobal(tabletGlobalPosition);
    const QPointF fallbackCanvasPosition = tabletCanvasPosition + QPointF(80.0, 0.0);
    const QPointF fallbackGlobalPosition = tabletFocusCanvas->mapToGlobal(fallbackCanvasPosition);
    const QPointF fallbackTargetPosition = tabletFocusBrushSize->mapFromGlobal(fallbackGlobalPosition);
    const QPointingDevice *tabletDevice = QPointingDevice::primaryPointingDevice();
    QTabletEvent tabletPress(QEvent::TabletPress,
                             tabletDevice,
                             tabletTargetPosition,
                             tabletGlobalPosition,
                             1.0,
                             0.0F,
                             0.0F,
                             0.0F,
                             0.0,
                             0.0F,
                             Qt::NoModifier,
                             Qt::LeftButton,
                             Qt::LeftButton);
    QApplication::sendEvent(tabletFocusBrushSize, &tabletPress);
    QTabletEvent tabletRelease(QEvent::TabletRelease,
                               tabletDevice,
                               tabletTargetPosition,
                               tabletGlobalPosition,
                               0.0,
                               0.0F,
                               0.0F,
                               0.0F,
                               0.0,
                               0.0F,
                               Qt::NoModifier,
                               Qt::LeftButton,
                               Qt::NoButton);
    QApplication::sendEvent(tabletFocusBrushSize, &tabletRelease);
    if (tabletFocusCanvas->document().layers()[0].image == beforeTabletFocusStroke) {
        return fail(QStringLiteral("tablet input over the canvas was not routed away from focused tool options"));
    }
    if (tabletFocusCanvas->commandManager()->undoHistory().isEmpty()) {
        return fail(QStringLiteral("tablet input routed from tool options did not finish a canvas stroke"));
    }

    resetTabletFocusCanvas();
    const QImage beforeModalTabletStroke = tabletFocusCanvas->document().layers()[0].image;
    {
        QDialog modalBlocker(&tabletFocusWindow);
        modalBlocker.setWindowModality(Qt::ApplicationModal);
        modalBlocker.show();
        modalBlocker.raise();
        modalBlocker.activateWindow();
        QApplication::processEvents();
        QTabletEvent blockedTabletPress(QEvent::TabletPress,
                                        tabletDevice,
                                        tabletTargetPosition,
                                        tabletGlobalPosition,
                                        1.0,
                                        0.0F,
                                        0.0F,
                                        0.0F,
                                        0.0,
                                        0.0F,
                                        Qt::NoModifier,
                                        Qt::LeftButton,
                                        Qt::LeftButton);
        QApplication::sendEvent(tabletFocusBrushSize, &blockedTabletPress);
        modalBlocker.close();
        QApplication::processEvents();
    }
    QApplication::processEvents();
    if (tabletFocusCanvas->document().layers()[0].image != beforeModalTabletStroke) {
        return fail(QStringLiteral("modal dialog did not block redirected tablet input to the canvas"));
    }

    QThread::msleep(90);
    QApplication::processEvents();

    resetTabletFocusCanvas();
    const QImage beforeMouseFocusStroke = tabletFocusCanvas->document().layers()[0].image;
    tabletFocusBrushSize->setFocus(Qt::OtherFocusReason);
    QMouseEvent redirectedMousePress(QEvent::MouseButtonPress,
                                     fallbackTargetPosition,
                                     fallbackTargetPosition,
                                     fallbackGlobalPosition,
                                     Qt::LeftButton,
                                     Qt::LeftButton,
                                     Qt::NoModifier);
    QApplication::sendEvent(tabletFocusBrushSize, &redirectedMousePress);
    QMouseEvent redirectedMouseRelease(QEvent::MouseButtonRelease,
                                       fallbackTargetPosition,
                                       fallbackTargetPosition,
                                       fallbackGlobalPosition,
                                       Qt::LeftButton,
                                       Qt::NoButton,
                                       Qt::NoModifier);
    QApplication::sendEvent(tabletFocusBrushSize, &redirectedMouseRelease);
    if (tabletFocusCanvas->document().layers()[0].image == beforeMouseFocusStroke) {
        return fail(QStringLiteral("mouse fallback input over the canvas was not routed away from focused tool options"));
    }

    resetTabletFocusCanvas();
    const QImage beforeOutsideStartStroke = tabletFocusCanvas->document().layers()[0].image;
    const QPointF outsideGlobalPosition = tabletFocusBrushSize->mapToGlobal(QPointF(tabletFocusBrushSize->rect().center()));
    const QPointF outsideTargetPosition = tabletFocusBrushSize->mapFromGlobal(outsideGlobalPosition);
    QMouseEvent outsideMousePress(QEvent::MouseButtonPress,
                                  outsideTargetPosition,
                                  outsideTargetPosition,
                                  outsideGlobalPosition,
                                  Qt::LeftButton,
                                  Qt::LeftButton,
                                  Qt::NoModifier);
    QApplication::sendEvent(tabletFocusBrushSize, &outsideMousePress);
    QMouseEvent outsideMouseMoveToCanvas(QEvent::MouseMove,
                                         fallbackTargetPosition,
                                         fallbackTargetPosition,
                                         fallbackGlobalPosition,
                                         Qt::NoButton,
                                         Qt::LeftButton,
                                         Qt::NoModifier);
    QApplication::sendEvent(tabletFocusBrushSize, &outsideMouseMoveToCanvas);
    QMouseEvent outsideMouseReleaseOnCanvas(QEvent::MouseButtonRelease,
                                            fallbackTargetPosition,
                                            fallbackTargetPosition,
                                            fallbackGlobalPosition,
                                            Qt::LeftButton,
                                            Qt::NoButton,
                                            Qt::NoModifier);
    QApplication::sendEvent(tabletFocusBrushSize, &outsideMouseReleaseOnCanvas);
    if (tabletFocusCanvas->document().layers()[0].image == beforeOutsideStartStroke) {
        return fail(QStringLiteral("pressed pointer entering the canvas did not start a routed stroke"));
    }

    resetTabletFocusCanvas();
    const QImage beforeTouchFocusStroke = tabletFocusCanvas->document().layers()[0].image;
    tabletFocusBrushSize->setAttribute(Qt::WA_AcceptTouchEvents, true);
    QTouchEvent redirectedTouchBegin(QEvent::TouchBegin,
                                     tabletDevice,
                                     Qt::NoModifier,
                                     QList<QEventPoint>{QEventPoint(1, QEventPoint::State::Pressed, fallbackTargetPosition, fallbackGlobalPosition)});
    QApplication::sendEvent(tabletFocusBrushSize, &redirectedTouchBegin);
    QTouchEvent redirectedTouchEnd(QEvent::TouchEnd,
                                   tabletDevice,
                                   Qt::NoModifier,
                                   QList<QEventPoint>{QEventPoint(1, QEventPoint::State::Released, fallbackTargetPosition, fallbackGlobalPosition)});
    QApplication::sendEvent(tabletFocusBrushSize, &redirectedTouchEnd);
    if (tabletFocusCanvas->document().layers()[0].image == beforeTouchFocusStroke) {
        const QPointF touchLocal = tabletFocusCanvas->mapFromGlobal(fallbackGlobalPosition);
        return fail(QStringLiteral("touch fallback input over the canvas was not converted into a canvas stroke; points=%1 local=%2,%3")
            .arg(redirectedTouchBegin.points().size())
            .arg(touchLocal.x())
            .arg(touchLocal.y()));
    }

    CanvasDocument layerIndexDoc;
    NewDocumentSettings layerIndexSettings;
    layerIndexSettings.width = 4;
    layerIndexSettings.height = 4;
    layerIndexSettings.transparentBackground = true;
    layerIndexSettings.defaultLayerCount = 3;
    layerIndexDoc.reset(layerIndexSettings);
    layerIndexDoc.setActiveLayerIndex(2);
    const QString activeLayerId = layerIndexDoc.layers()[2].id;
    if (!layerIndexDoc.deleteLayer(0)
        || layerIndexDoc.activeLayerIndex() != 1
        || layerIndexDoc.layers()[layerIndexDoc.activeLayerIndex()].id != activeLayerId) {
        return fail(QStringLiteral("deleting a lower layer did not preserve the active logical layer"));
    }

    CanvasDocument hiddenBackgroundDoc;
    NewDocumentSettings hiddenBackgroundSettings;
    hiddenBackgroundSettings.width = 2;
    hiddenBackgroundSettings.height = 2;
    hiddenBackgroundSettings.transparentBackground = false;
    hiddenBackgroundSettings.backgroundColor = Qt::white;
    hiddenBackgroundSettings.defaultLayerCount = 1;
    hiddenBackgroundDoc.reset(hiddenBackgroundSettings);
    hiddenBackgroundDoc.setLayerVisible(0, false);
    if (qAlpha(hiddenBackgroundDoc.compositedImage(true).pixel(0, 0)) != 0) {
        return fail(QStringLiteral("hidden background layer still contributed implicit background pixels"));
    }

    CanvasDocument hiddenMergeDoc;
    hiddenMergeDoc.reset(noOpSettings);
    hiddenMergeDoc.activeImage()->fill(QColor(255, 0, 0, 255));
    hiddenMergeDoc.addLayer(QStringLiteral("Hidden Blue"));
    hiddenMergeDoc.activeImage()->fill(QColor(0, 0, 255, 255));
    hiddenMergeDoc.setLayerVisible(hiddenMergeDoc.activeLayerIndex(), false);
    const QImage belowBeforeHiddenMerge = hiddenMergeDoc.layers()[0].image;
    if (!hiddenMergeDoc.mergeLayerDown(hiddenMergeDoc.activeLayerIndex())
        || hiddenMergeDoc.layers()[0].image != belowBeforeHiddenMerge) {
        return fail(QStringLiteral("mergeLayerDown merged hidden layer pixels"));
    }

    QImage toleranceImage(3, 1, QImage::Format_ARGB32_Premultiplied);
    toleranceImage.setPixelColor(0, 0, QColor(100, 100, 100, 255));
    toleranceImage.setPixelColor(1, 0, QColor(108, 94, 105, 255));
    toleranceImage.setPixelColor(2, 0, QColor(120, 100, 100, 255));
    FillSettings toleranceSettings;
    toleranceSettings.tolerance = 10;
    toleranceSettings.contiguous = false;
    const FillRegion toleranceRegion = FillEngine::matchedRegion(toleranceImage, QPoint(0, 0), toleranceSettings);
    if (toleranceRegion.pixelIndexes.size() != 2) {
        return fail(QStringLiteral("fill tolerance did not match per-channel RGB/alpha expectations"));
    }

    QImage fillFringeImage(6, 3, QImage::Format_ARGB32_Premultiplied);
    fillFringeImage.fill(Qt::transparent);
    fillFringeImage.setPixelColor(2, 1, QColor(0, 0, 0, 64));
    fillFringeImage.setPixelColor(3, 1, QColor(0, 0, 0, 128));
    fillFringeImage.setPixelColor(4, 1, QColor(0, 0, 0, 255));
    FillSettings fringeSettings;
    fringeSettings.tolerance = 0;
    fringeSettings.contiguous = true;
    const QRgb opaqueStrokeBefore = fillFringeImage.pixel(4, 1);
    const FillResult fringeFill = FillEngine::floodFill(fillFringeImage,
                                                        QPoint(0, 1),
                                                        QColor(255, 0, 0, 255),
                                                        fringeSettings);
    if (!fringeFill.hasChanges()) {
        return fail(QStringLiteral("fill fringe regression produced no changes"));
    }
    const QColor antialiasPixel = QColor::fromRgba(fillFringeImage.pixel(2, 1));
    if (antialiasPixel.alpha() <= 128 || antialiasPixel.red() <= 0) {
        return fail(QStringLiteral("fill did not composite behind semi-transparent brush fringe"));
    }
    const QColor deeperAntialiasPixel = QColor::fromRgba(fillFringeImage.pixel(3, 1));
    if (deeperAntialiasPixel.alpha() <= 128 || deeperAntialiasPixel.red() <= 0) {
        return fail(QStringLiteral("fill did not propagate underpaint through multi-pixel brush fringe"));
    }
    if (fillFringeImage.pixel(4, 1) != opaqueStrokeBefore) {
        return fail(QStringLiteral("fill overwrote an opaque stroke boundary"));
    }

    QImage limitedFringeImage(7, 1, QImage::Format_ARGB32_Premultiplied);
    limitedFringeImage.fill(Qt::transparent);
    limitedFringeImage.setPixelColor(2, 0, QColor(0, 0, 0, 64));
    limitedFringeImage.setPixelColor(3, 0, QColor(0, 0, 0, 128));
    limitedFringeImage.setPixelColor(4, 0, QColor(0, 0, 0, 128));
    limitedFringeImage.setPixelColor(5, 0, QColor(0, 0, 0, 255));
    const QRgb thirdFringeBefore = limitedFringeImage.pixel(4, 0);
    FillEngine::floodFill(limitedFringeImage, QPoint(0, 0), QColor(255, 0, 0, 255), fringeSettings);
    if (limitedFringeImage.pixel(4, 0) != thirdFringeBefore) {
        return fail(QStringLiteral("fill underpaint propagated beyond the bounded anti-alias fringe"));
    }

    QImage largeTextImage(320, 160, QImage::Format_ARGB32_Premultiplied);
    largeTextImage.fill(Qt::transparent);
    TextSettings largeTextSettings;
    largeTextSettings.font = QApplication::font();
    largeTextSettings.font.setPointSize(72);
    largeTextSettings.color = QColor(0, 0, 0, 255);
    largeTextSettings.boxSize = QSizeF(260, 120);
    const TextRenderResult largeTextResult = TextEngine::drawTextBox(largeTextImage,
                                                                     QPointF(12, 12),
                                                                     QStringLiteral("Text"),
                                                                     largeTextSettings);
    if (!largeTextResult.changed) {
        return fail(QStringLiteral("large text settings did not render"));
    }
    bool foundTextPixel = false;
    for (int y = 0; y < largeTextImage.height() && !foundTextPixel; ++y) {
        for (int x = 0; x < largeTextImage.width(); ++x) {
            if (qAlpha(largeTextImage.pixel(x, y)) > 0) {
                foundTextPixel = true;
                break;
            }
        }
    }
    if (!foundTextPixel) {
        return fail(QStringLiteral("large text rendered no visible pixels"));
    }

    NewDocumentSettings metadataSettings;
    metadataSettings.width = 8;
    metadataSettings.height = 6;
    metadataSettings.dpi = 144;
    metadataSettings.backgroundColor = QColor(18, 52, 86, 255);
    metadataSettings.transparentBackground = false;
    metadataSettings.colorSpace = QStringLiteral("Display P3");
    metadataSettings.bitDepth = 16;
    metadataSettings.defaultLayerCount = 1;

    CanvasDocument metadataDoc;
    metadataDoc.reset(metadataSettings);
    if (qAlpha(metadataDoc.compositedImage(false).pixel(0, 0)) != 0) {
        return fail(QStringLiteral("includeBackground=false did not omit implicit background layer"));
    }

    const QString projectPath = temp.filePath(QStringLiteral("metadata.hxp"));
    QString error;
    if (!ProjectSerializer::save(metadataDoc, projectPath, &error)) {
        return fail(QStringLiteral("metadata save failed: %1").arg(error));
    }
    CanvasDocument loadedMetadata;
    if (!ProjectSerializer::load(loadedMetadata, projectPath, &error)) {
        return fail(QStringLiteral("metadata load failed: %1").arg(error));
    }
    const DocumentState loadedState = loadedMetadata.snapshot();
    if (loadedState.dpi != 144
        || loadedState.backgroundColor != QColor(18, 52, 86, 255)
        || loadedState.colorSpace != QStringLiteral("Display P3")
        || loadedState.bitDepth != 16) {
        return fail(QStringLiteral("project metadata was not preserved"));
    }

    NewDocumentSettings blendSettings;
    blendSettings.width = 1;
    blendSettings.height = 1;
    blendSettings.transparentBackground = true;
    blendSettings.defaultLayerCount = 1;
    CanvasDocument blendDoc;
    blendDoc.reset(blendSettings);
    blendDoc.activeImage()->fill(QColor(255, 0, 0, 255));
    blendDoc.addLayer(QStringLiteral("Multiply Blue"));
    blendDoc.activeImage()->fill(QColor(0, 0, 255, 255));
    blendDoc.setLayerBlendMode(blendDoc.activeLayerIndex(), QStringLiteral("Multiply"));
    const QColor blendPixel = QColor::fromRgba(blendDoc.compositedImage().pixel(0, 0));
    if (blendPixel.red() > 16 || blendPixel.green() > 16 || blendPixel.blue() > 16) {
        return fail(QStringLiteral("multiply blend mode was not applied"));
    }
    if (!blendDoc.mergeLayerDown(blendDoc.activeLayerIndex())) {
        return fail(QStringLiteral("mergeLayerDown failed"));
    }
    const QColor mergedBlendPixel = QColor::fromRgba(blendDoc.activeImage()->pixel(0, 0));
    if (mergedBlendPixel.red() > 16 || mergedBlendPixel.green() > 16 || mergedBlendPixel.blue() > 16) {
        return fail(QStringLiteral("mergeLayerDown did not preserve blend mode result"));
    }

    QImage pasted(4, 4, QImage::Format_ARGB32_Premultiplied);
    pasted.fill(QColor(0, 180, 255, 200));
    const int beforeAdd = blendDoc.layers().size();
    if (!blendDoc.addImageLayer(pasted, QStringLiteral("Pasted Image"), &error) || blendDoc.layers().size() != beforeAdd + 1) {
        return fail(QStringLiteral("addImageLayer failed: %1").arg(error));
    }

    ExportOptions jpgOptions;
    jpgOptions.filePath = temp.filePath(QStringLiteral("format-mismatch.png"));
    jpgOptions.format = QStringLiteral("JPG");
    jpgOptions.includeBackground = true;
    if (!ExportManager::exportDocument(metadataDoc, jpgOptions, &error)) {
        return fail(QStringLiteral("JPG export failed: %1").arg(error));
    }
    QFile jpgFile(jpgOptions.filePath);
    if (!jpgFile.open(QIODevice::ReadOnly) || jpgFile.read(2) != QByteArray::fromHex("ffd8")) {
        return fail(QStringLiteral("ExportOptions::format was not respected for JPG"));
    }

    ExportOptions resizedOptions;
    resizedOptions.filePath = temp.filePath(QStringLiteral("resized.png"));
    resizedOptions.format = QStringLiteral("PNG");
    resizedOptions.resizeEnabled = true;
    resizedOptions.targetSize = QSize(3, 4);
    if (!ExportManager::exportDocument(metadataDoc, resizedOptions, &error)) {
        return fail(QStringLiteral("resized export failed: %1").arg(error));
    }
    QImage resized(resizedOptions.filePath);
    if (resized.size() != QSize(3, 4)) {
        return fail(QStringLiteral("export resize option was not applied"));
    }

    CanvasDocument activeOnlyDoc;
    activeOnlyDoc.reset(blendSettings);
    activeOnlyDoc.activeImage()->fill(QColor(255, 0, 0, 255));
    activeOnlyDoc.addLayer(QStringLiteral("Top Blue"));
    activeOnlyDoc.activeImage()->fill(QColor(0, 0, 255, 255));
    activeOnlyDoc.setActiveLayerIndex(0);
    ExportOptions activeOnlyOptions;
    activeOnlyOptions.filePath = temp.filePath(QStringLiteral("active-only.png"));
    activeOnlyOptions.format = QStringLiteral("PNG");
    activeOnlyOptions.mergeLayers = false;
    activeOnlyOptions.includeBackground = false;
    if (!ExportManager::exportDocument(activeOnlyDoc, activeOnlyOptions, &error)) {
        return fail(QStringLiteral("active-only export failed: %1").arg(error));
    }
    const QColor activeOnlyPixel = QColor::fromRgba(QImage(activeOnlyOptions.filePath).pixel(0, 0));
    if (activeOnlyPixel.red() < 240 || activeOnlyPixel.blue() > 16) {
        return fail(QStringLiteral("mergeLayers=false did not export the active layer only"));
    }

    CanvasDocument activeOpacityDoc;
    activeOpacityDoc.reset(blendSettings);
    activeOpacityDoc.activeImage()->fill(QColor(255, 0, 0, 255));
    activeOpacityDoc.setLayerOpacity(activeOpacityDoc.activeLayerIndex(), 0.5);
    ExportOptions activeOpacityOptions;
    activeOpacityOptions.filePath = temp.filePath(QStringLiteral("active-opacity.png"));
    activeOpacityOptions.format = QStringLiteral("PNG");
    activeOpacityOptions.mergeLayers = false;
    activeOpacityOptions.includeBackground = false;
    if (!ExportManager::exportDocument(activeOpacityDoc, activeOpacityOptions, &error)) {
        return fail(QStringLiteral("active opacity export failed: %1").arg(error));
    }
    const QColor activeOpacityPixel = QColor::fromRgba(QImage(activeOpacityOptions.filePath).pixel(0, 0));
    if (activeOpacityPixel.alpha() < 110 || activeOpacityPixel.alpha() > 150) {
        return fail(QStringLiteral("mergeLayers=false did not preserve active layer opacity"));
    }

    OpenGLCanvasWidget transformCanvas;
    NewDocumentSettings transformSettings;
    transformSettings.width = 16;
    transformSettings.height = 16;
    transformSettings.transparentBackground = true;
    transformSettings.defaultLayerCount = 1;
    transformCanvas.newDocument(transformSettings);
    QImage *transformImage = transformCanvas.document().activeImage();
    if (!transformImage) {
        return fail(QStringLiteral("selection transform test had no active image"));
    }
    transformImage->fill(Qt::transparent);
    {
        QPainter painter(transformImage);
        painter.fillRect(QRect(4, 4, 4, 4), QColor(255, 0, 0, 255));
    }
    const QImage beforeTransform = *transformImage;
    transformCanvas.setSelectionRect(QRect(4, 4, 4, 4));
    transformCanvas.transformSelection(45.0, 8, 4, 100);
    const QRect transformedSelection = transformCanvas.selectionRect();
    if (!transformedSelection.isValid() || transformedSelection == QRect(4, 4, 4, 4)) {
        return fail(QStringLiteral("selection transform did not update the selection bounds"));
    }
    if (transformCanvas.document().layers()[0].image == beforeTransform) {
        return fail(QStringLiteral("selection transform did not change pixels"));
    }
    transformCanvas.commandManager()->undo();
    if (transformCanvas.document().layers()[0].image != beforeTransform) {
        return fail(QStringLiteral("selection transform undo did not restore pixels"));
    }
    if (transformCanvas.selectionRect() != QRect(4, 4, 4, 4)) {
        return fail(QStringLiteral("selection transform undo did not restore selection bounds"));
    }
    transformCanvas.commandManager()->redo();
    if (transformCanvas.selectionRect() != transformedSelection) {
        return fail(QStringLiteral("selection transform redo did not restore selection bounds"));
    }

    qInfo().noquote() << "Functional regression smoke test passed";
    qInfo().noquote() << "Checked: mouse press tool input, no-op command filtering, transparent brush/eraser no-op detection, widget stroke undo grouping, layer active-index preservation, hidden background/merge behavior, fill tolerance, fill anti-alias fringe compositing, bounded fill underpaint, large text rendering, project metadata, includeBackground, blend mode merge, addImageLayer, export format, export resize, active-layer-only export, active-layer opacity export, selection vector transform undo/redo";
    return 0;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("HXPainter");
    QApplication::setOrganizationName("HXPainter");
    AppTheme::apply(app);

    const IconLoadResult iconResult = IconLoader::loadApplicationIconWithDiagnostics();
    if (!iconResult.icon.isNull()) {
        QApplication::setWindowIcon(iconResult.icon);
    }

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.contains(QStringLiteral("--smoke-icon-test"))) {
        const bool logoExistsBesideExecutable = QFileInfo::exists(AppPaths::logoPath());
        const bool iconLoaded = !iconResult.icon.isNull();
        const bool appIconApplied = !QApplication::windowIcon().isNull();

        qInfo().noquote() << "HXPainter icon smoke test";
        qInfo().noquote() << "Executable dir:" << AppPaths::executableDir();
        qInfo().noquote() << "Expected logo:" << AppPaths::logoPath();
        qInfo().noquote() << "Logo exists beside executable:" << logoExistsBesideExecutable;
        qInfo().noquote() << "Icon source:" << iconResult.sourcePath;
        qInfo().noquote() << "QApplication icon applied:" << appIconApplied;

        return logoExistsBesideExecutable && iconLoaded && appIconApplied ? 0 : 10;
    }

    if (arguments.contains(QStringLiteral("--mvp-smoke-test"))) {
        return runMvpSmokeTest();
    }

    if (arguments.contains(QStringLiteral("--theme-smoke-test"))) {
        return runThemeSmokeTest();
    }

    if (arguments.contains(QStringLiteral("--functional-regression-smoke-test"))) {
        return runFunctionalRegressionSmokeTest();
    }

    MainWindow window;
    if (!iconResult.icon.isNull()) {
        window.setWindowIcon(iconResult.icon);
    }
    window.setStartupDiagnostics(iconResult.messages);
    window.resize(1280, 820);
    window.show();

    return QApplication::exec();
}

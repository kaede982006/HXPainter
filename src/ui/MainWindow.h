#pragma once

#include "actions/ActionRegistry.h"
#include "render/OpenGLCanvasWidget.h"
#include "tools/ToolManager.h"
#include "ui/panels/ToolOptionsPanel.h"

#include <QComboBox>
#include <QDockWidget>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>

class QCloseEvent;
class QEvent;
class QMenu;
class QMouseEvent;
class QTabletEvent;
class QTouchEvent;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setStartupDiagnostics(const QStringList &messages);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildMenus();
    void buildToolBars();
    void buildDocks();
    void buildStatusBar();
    void connectActions();
    void connectDocumentSignals();

    void showCreateOpenHub();
    void openFile(const QString &path);
    void importFile(const QString &path);
    void importReferenceFile(const QString &path);
    void save();
    void saveAs();
    bool saveDocument();
    bool saveDocumentAs();
    bool maybeSaveModified();
    void exportImage();
    void closeDocument();
    void clearActiveLayer();
    void copyActiveLayer();
    void cutActiveLayer();
    void pasteImageFromClipboard();
    void showPreferences();
    void showFilterGallery();

    void refreshUiState();
    void refreshLayers();
    void refreshHistory();
    void refreshNavigator();
    void refreshStatus();
    void refreshDiagnostics();
    void updateColorControls(const QColor &color);
    void setForegroundFromControls();
    void chooseForegroundColor();
    void chooseBackgroundColor();
    void applyBrushPreset(QListWidgetItem *item);
    void showLayerOpacityDialog();
    void renameActiveLayer();
    void toggleActiveLayerVisibility();
    void toggleActiveLayerLock();
    void toggleActiveLayerAlphaLock();
    void showBlendModeDialog();
    void showMessage(const QString &message);
    bool canvasContainsGlobalPosition(const QPointF &globalPosition, QPointF *canvasPosition = nullptr) const;
    void focusCanvasIfPointerOverCanvas();
    bool shouldBlockCanvasRedirect() const;
    bool shouldBlockCanvasRedirect(const QPointF &globalPos) const;
    bool routeCanvasMouseEvent(QObject *target, QMouseEvent *event);
    bool routeCanvasTabletEvent(QObject *target, QTabletEvent *event);
    bool routeCanvasTouchEvent(QObject *target, QTouchEvent *event);

    OpenGLCanvasWidget *canvas_ = nullptr;
    ActionRegistry *actions_ = nullptr;
    ToolManager *toolManager_ = nullptr;
    ToolOptionsPanel *toolOptionsPanel_ = nullptr;

    QDockWidget *layersDock_ = nullptr;
    QDockWidget *colorDock_ = nullptr;
    QDockWidget *brushPresetsDock_ = nullptr;
    QDockWidget *toolOptionsDock_ = nullptr;
    QDockWidget *historyDock_ = nullptr;
    QDockWidget *navigatorDock_ = nullptr;
    QDockWidget *performanceDock_ = nullptr;
    QMenu *windowMenu_ = nullptr;

    QListWidget *layersList_ = nullptr;
    QListWidget *historyList_ = nullptr;
    QListWidget *brushPresetsList_ = nullptr;
    QLabel *navigatorLabel_ = nullptr;
    QLabel *diagnosticsLabel_ = nullptr;
    QPushButton *foregroundButton_ = nullptr;
    QPushButton *backgroundButton_ = nullptr;
    QSpinBox *redSpin_ = nullptr;
    QSpinBox *greenSpin_ = nullptr;
    QSpinBox *blueSpin_ = nullptr;
    QSpinBox *alphaSpin_ = nullptr;
    QSlider *layerOpacitySlider_ = nullptr;
    QComboBox *blendModeCombo_ = nullptr;

    QLabel *docSizeStatus_ = nullptr;
    QLabel *zoomStatus_ = nullptr;
    QLabel *cursorStatus_ = nullptr;
    QLabel *toolStatus_ = nullptr;
    QLabel *layerStatus_ = nullptr;
    QLabel *brushStatus_ = nullptr;
    QLabel *colorStatus_ = nullptr;
    QLabel *layerOpacityStatus_ = nullptr;
    QLabel *fpsStatus_ = nullptr;

    QTimer diagnosticsTimer_;
    QStringList startupDiagnostics_;
    bool updatingColorControls_ = false;
    bool startupHubShown_ = false;
    bool mouseCanvasRedirectActive_ = false;
    bool tabletCanvasRedirectActive_ = false;
    bool touchCanvasRedirectActive_ = false;
};

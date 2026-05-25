#pragma once

#include "render/CanvasDocument.h"

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QTabWidget>

class CreateOpenHub final : public QDialog {
    Q_OBJECT

public:
    enum class Mode {
        None,
        NewDocument,
        OpenExisting,
        ImportImage
    };

    enum class ImportMode {
        ImportAsNewLayer,
        OpenAsNewDocument,
        ImportAsReferenceImage
    };

    explicit CreateOpenHub(QWidget *parent = nullptr);

    static QStringList recentProjectPaths();
    static void rememberSavedProject(const QString &filePath);
    static void rememberLoadedProject(const QString &filePath);

    [[nodiscard]] Mode mode() const;
    [[nodiscard]] ImportMode importMode() const;
    [[nodiscard]] NewDocumentSettings newDocumentSettings() const;
    [[nodiscard]] QString selectedFilePath() const;

private:
    void buildNewTab();
    void buildOpenTab();
    void buildImportTab();
    void buildRecentTab();
    void buildTemplatesTab();
    void applyTemplate(const QString &name);
    void selectTemplateItem(QListWidgetItem *item);
    [[nodiscard]] QString selectedRecentProjectPath() const;
    void chooseOpenFile();
    void chooseImportFile();

    QTabWidget *tabs_ = nullptr;
    QComboBox *templateCombo_ = nullptr;
    QSpinBox *widthSpin_ = nullptr;
    QSpinBox *heightSpin_ = nullptr;
    QSpinBox *dpiSpin_ = nullptr;
    QPushButton *backgroundButton_ = nullptr;
    QCheckBox *transparentCheck_ = nullptr;
    QComboBox *colorSpaceCombo_ = nullptr;
    QComboBox *bitDepthCombo_ = nullptr;
    QSpinBox *layerCountSpin_ = nullptr;
    QLineEdit *openPathEdit_ = nullptr;
    QLineEdit *importPathEdit_ = nullptr;
    QComboBox *importModeCombo_ = nullptr;
    QListWidget *recentProjectsList_ = nullptr;
    QListWidget *templatesList_ = nullptr;
    QColor backgroundColor_ = Qt::white;
    Mode mode_ = Mode::None;
    QString selectedFilePath_;
};

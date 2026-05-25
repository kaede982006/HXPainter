#include "CreateOpenHub.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int MaxRecentProjects = 10;

QString recentProjectsSettingsKey()
{
    return QStringLiteral("recentProjects");
}

QString savedProjectsSettingsKey()
{
    return QStringLiteral("recentSavedProjects");
}

QString loadedProjectsSettingsKey()
{
    return QStringLiteral("recentLoadedProjects");
}

QString normalizedProjectPath(const QString &filePath, bool requireExisting)
{
    const QString trimmed = filePath.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QFileInfo info(trimmed);
    if (requireExisting && (!info.exists() || !info.isFile())) {
        return {};
    }
    if (info.suffix().compare(QStringLiteral("hxp"), Qt::CaseInsensitive) != 0) {
        return {};
    }

    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool containsPath(const QStringList &paths, const QString &path)
{
    return std::any_of(paths.cbegin(), paths.cend(), [&path](const QString &existing) {
        return QString::compare(existing, path, Qt::CaseInsensitive) == 0;
    });
}

QStringList normalizedStoredProjectPaths(const QString &settingsKey)
{
    const QStringList stored = QSettings().value(settingsKey).toStringList();
    QStringList paths;
    paths.reserve(stored.size());
    for (const QString &storedPath : stored) {
        const QString path = normalizedProjectPath(storedPath, false);
        if (!path.isEmpty() && !containsPath(paths, path)) {
            paths.push_back(path);
        }
    }

    if (paths.size() > MaxRecentProjects) {
        paths = paths.mid(0, MaxRecentProjects);
    }
    if (paths != stored) {
        QSettings().setValue(settingsKey, paths);
    }
    return paths;
}

void prependProjectPath(const QString &settingsKey, const QString &path)
{
    QStringList paths = normalizedStoredProjectPaths(settingsKey);
    for (int i = paths.size() - 1; i >= 0; --i) {
        if (QString::compare(paths.at(i), path, Qt::CaseInsensitive) == 0) {
            paths.removeAt(i);
        }
    }
    paths.prepend(path);
    if (paths.size() > MaxRecentProjects) {
        paths = paths.mid(0, MaxRecentProjects);
    }

    QSettings().setValue(settingsKey, paths);
}

void rememberProjectHistory(const QString &filePath, const QString &historyKey)
{
    const QString path = normalizedProjectPath(filePath, true);
    if (path.isEmpty()) {
        return;
    }

    prependProjectPath(historyKey, path);
    prependProjectPath(recentProjectsSettingsKey(), path);
}
}

CreateOpenHub::CreateOpenHub(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Create / Open"));
    resize(680, 520);

    tabs_ = new QTabWidget(this);
    buildNewTab();
    buildOpenTab();
    buildImportTab();
    buildRecentTab();
    buildTemplatesTab();

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    QPushButton *createButton = buttons->addButton(QStringLiteral("Create"), QDialogButtonBox::AcceptRole);
    QPushButton *openButton = buttons->addButton(QStringLiteral("Open"), QDialogButtonBox::ActionRole);
    QPushButton *importButton = buttons->addButton(QStringLiteral("Import"), QDialogButtonBox::ActionRole);

    QObject::connect(createButton, &QPushButton::clicked, this, [this] {
        if (tabs_->currentWidget() == templatesList_) {
            selectTemplateItem(templatesList_->currentItem());
        }
        mode_ = Mode::NewDocument;
        accept();
    });
    QObject::connect(openButton, &QPushButton::clicked, this, [this] {
        selectedFilePath_ = tabs_->currentWidget() == recentProjectsList_
            ? selectedRecentProjectPath()
            : openPathEdit_->text().trimmed();
        if (selectedFilePath_.isEmpty()) {
            chooseOpenFile();
        }
        if (!selectedFilePath_.isEmpty()) {
            mode_ = Mode::OpenExisting;
            accept();
        }
    });
    QObject::connect(importButton, &QPushButton::clicked, this, [this] {
        selectedFilePath_ = importPathEdit_->text().trimmed();
        if (selectedFilePath_.isEmpty()) {
            chooseImportFile();
        }
        if (!selectedFilePath_.isEmpty()) {
            mode_ = Mode::ImportImage;
            accept();
        }
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(tabs_);
    layout->addWidget(buttons);
}

QStringList CreateOpenHub::recentProjectPaths()
{
    QStringList paths;
    const QList<QStringList> sources = {
        normalizedStoredProjectPaths(recentProjectsSettingsKey()),
        normalizedStoredProjectPaths(savedProjectsSettingsKey()),
        normalizedStoredProjectPaths(loadedProjectsSettingsKey())
    };

    for (const QStringList &source : sources) {
        for (const QString &path : source) {
            if (!containsPath(paths, path)) {
                paths.push_back(path);
                if (paths.size() == MaxRecentProjects) {
                    break;
                }
            }
        }
        if (paths.size() == MaxRecentProjects) {
            break;
        }
    }

    QSettings().setValue(recentProjectsSettingsKey(), paths);
    return paths;
}

void CreateOpenHub::rememberSavedProject(const QString &filePath)
{
    rememberProjectHistory(filePath, savedProjectsSettingsKey());
}

void CreateOpenHub::rememberLoadedProject(const QString &filePath)
{
    rememberProjectHistory(filePath, loadedProjectsSettingsKey());
}

CreateOpenHub::Mode CreateOpenHub::mode() const
{
    return mode_;
}

CreateOpenHub::ImportMode CreateOpenHub::importMode() const
{
    const QString mode = importModeCombo_ ? importModeCombo_->currentText() : QString();
    if (mode == QStringLiteral("Open as New Document")) {
        return ImportMode::OpenAsNewDocument;
    }
    if (mode == QStringLiteral("Import as Reference Image")) {
        return ImportMode::ImportAsReferenceImage;
    }
    return ImportMode::ImportAsNewLayer;
}

NewDocumentSettings CreateOpenHub::newDocumentSettings() const
{
    NewDocumentSettings settings;
    settings.width = widthSpin_->value();
    settings.height = heightSpin_->value();
    settings.dpi = dpiSpin_->value();
    settings.backgroundColor = backgroundColor_;
    settings.transparentBackground = transparentCheck_->isChecked();
    settings.colorSpace = colorSpaceCombo_->currentText();
    settings.bitDepth = bitDepthCombo_->currentText().remove(QStringLiteral(" bit")).toInt();
    settings.defaultLayerCount = layerCountSpin_->value();
    settings.templateName = templateCombo_->currentText();
    return settings;
}

QString CreateOpenHub::selectedFilePath() const
{
    return selectedFilePath_;
}

void CreateOpenHub::buildNewTab()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);

    templateCombo_ = new QComboBox(page);
    templateCombo_->addItems({QStringLiteral("Default"), QStringLiteral("Illustration"), QStringLiteral("Webtoon"),
                              QStringLiteral("Texture"), QStringLiteral("Pixel Art"), QStringLiteral("Custom")});
    widthSpin_ = new QSpinBox(page);
    widthSpin_->setRange(1, 16384);
    widthSpin_->setValue(1920);
    heightSpin_ = new QSpinBox(page);
    heightSpin_->setRange(1, 16384);
    heightSpin_->setValue(1080);
    dpiSpin_ = new QSpinBox(page);
    dpiSpin_->setRange(1, 1200);
    dpiSpin_->setValue(300);
    backgroundButton_ = new QPushButton(QStringLiteral("White"), page);
    transparentCheck_ = new QCheckBox(QStringLiteral("Transparent background"), page);
    colorSpaceCombo_ = new QComboBox(page);
    colorSpaceCombo_->addItems({QStringLiteral("sRGB"), QStringLiteral("Display P3"), QStringLiteral("Linear RGB")});
    bitDepthCombo_ = new QComboBox(page);
    bitDepthCombo_->addItems({QStringLiteral("8 bit"), QStringLiteral("16 bit"), QStringLiteral("32 bit float")});
    layerCountSpin_ = new QSpinBox(page);
    layerCountSpin_->setRange(1, 32);
    layerCountSpin_->setValue(2);

    form->addRow(QStringLiteral("Template"), templateCombo_);
    form->addRow(QStringLiteral("Width"), widthSpin_);
    form->addRow(QStringLiteral("Height"), heightSpin_);
    form->addRow(QStringLiteral("DPI"), dpiSpin_);
    form->addRow(QStringLiteral("Background"), backgroundButton_);
    form->addRow(QString(), transparentCheck_);
    form->addRow(QStringLiteral("Color Space"), colorSpaceCombo_);
    form->addRow(QStringLiteral("Bit Depth"), bitDepthCombo_);
    form->addRow(QStringLiteral("Default Layers"), layerCountSpin_);

    QObject::connect(templateCombo_, &QComboBox::currentTextChanged, this, &CreateOpenHub::applyTemplate);
    QObject::connect(backgroundButton_, &QPushButton::clicked, this, [this] {
        const QColor selected = QColorDialog::getColor(backgroundColor_, this, QStringLiteral("Background Color"));
        if (selected.isValid()) {
            backgroundColor_ = selected;
            backgroundButton_->setText(selected.name(QColor::HexArgb));
        }
    });

    auto markCustom = [this] {
        if (templateCombo_->currentText() != QStringLiteral("Custom")) {
            templateCombo_->setCurrentText(QStringLiteral("Custom"));
        }
    };
    QObject::connect(widthSpin_, &QSpinBox::valueChanged, this, markCustom);
    QObject::connect(heightSpin_, &QSpinBox::valueChanged, this, markCustom);
    QObject::connect(layerCountSpin_, &QSpinBox::valueChanged, this, markCustom);

    tabs_->addTab(page, QStringLiteral("New Document"));
}

void CreateOpenHub::buildOpenTab()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(QStringLiteral("Open .hxp projects or standard image files as a new document."), page));
    openPathEdit_ = new QLineEdit(page);
    QPushButton *browse = new QPushButton(QStringLiteral("Choose File..."), page);
    layout->addWidget(openPathEdit_);
    layout->addWidget(browse);
    layout->addStretch();
    QObject::connect(browse, &QPushButton::clicked, this, &CreateOpenHub::chooseOpenFile);
    tabs_->addTab(page, QStringLiteral("Open Existing"));
}

void CreateOpenHub::buildImportTab()
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);
    importPathEdit_ = new QLineEdit(page);
    QPushButton *browse = new QPushButton(QStringLiteral("Choose Image..."), page);
    importModeCombo_ = new QComboBox(page);
    importModeCombo_->addItems({QStringLiteral("Import as New Layer"), QStringLiteral("Open as New Document"),
                                QStringLiteral("Import as Reference Image")});
    form->addRow(QStringLiteral("Mode"), importModeCombo_);
    form->addRow(QStringLiteral("Image"), importPathEdit_);
    form->addRow(QString(), browse);
    QObject::connect(browse, &QPushButton::clicked, this, &CreateOpenHub::chooseImportFile);
    tabs_->addTab(page, QStringLiteral("Import Image"));
}

void CreateOpenHub::buildRecentTab()
{
    recentProjectsList_ = new QListWidget(this);
    recentProjectsList_->setObjectName(QStringLiteral("recentProjectsList"));

    const QStringList paths = recentProjectPaths();
    if (paths.isEmpty()) {
        QListWidgetItem *empty = new QListWidgetItem(QStringLiteral("No recent projects yet"), recentProjectsList_);
        empty->setFlags(empty->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
    } else {
        for (const QString &path : paths) {
            const QFileInfo info(path);
            QListWidgetItem *item = new QListWidgetItem(
                QStringLiteral("%1\n%2").arg(info.fileName(), path), recentProjectsList_);
            item->setToolTip(path);
            item->setData(Qt::UserRole, path);
        }
    }

    QObject::connect(recentProjectsList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) {
            return;
        }
        selectedFilePath_ = path;
        mode_ = Mode::OpenExisting;
        accept();
    });

    tabs_->addTab(recentProjectsList_, QStringLiteral("Recent Projects"));
}

void CreateOpenHub::buildTemplatesTab()
{
    templatesList_ = new QListWidget(this);
    templatesList_->setObjectName(QStringLiteral("templatesList"));

    auto addTemplate = [this](const QString &name, int width, int height) {
        QListWidgetItem *item = new QListWidgetItem(
            QStringLiteral("%1 - %2 x %3").arg(name).arg(width).arg(height), templatesList_);
        item->setData(Qt::UserRole, name);
    };
    addTemplate(QStringLiteral("Default"), 1920, 1080);
    addTemplate(QStringLiteral("Illustration"), 3000, 3000);
    addTemplate(QStringLiteral("Webtoon"), 1600, 6000);
    addTemplate(QStringLiteral("Texture"), 2048, 2048);
    addTemplate(QStringLiteral("Pixel Art"), 512, 512);

    QObject::connect(templatesList_, &QListWidget::itemSelectionChanged, this, [this] {
        selectTemplateItem(templatesList_->currentItem());
    });
    QObject::connect(templatesList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        selectTemplateItem(item);
        mode_ = Mode::NewDocument;
        accept();
    });

    tabs_->addTab(templatesList_, QStringLiteral("Templates"));
}

void CreateOpenHub::applyTemplate(const QString &name)
{
    const QSignalBlocker blockWidth(widthSpin_);
    const QSignalBlocker blockHeight(heightSpin_);
    const QSignalBlocker blockLayers(layerCountSpin_);

    if (name == QStringLiteral("Illustration")) {
        widthSpin_->setValue(3000);
        heightSpin_->setValue(3000);
        dpiSpin_->setValue(300);
        layerCountSpin_->setValue(3);
        backgroundColor_ = Qt::white;
        transparentCheck_->setChecked(false);
    } else if (name == QStringLiteral("Webtoon")) {
        widthSpin_->setValue(1600);
        heightSpin_->setValue(6000);
        dpiSpin_->setValue(300);
        layerCountSpin_->setValue(4);
        backgroundColor_ = Qt::white;
        transparentCheck_->setChecked(false);
    } else if (name == QStringLiteral("Texture")) {
        widthSpin_->setValue(2048);
        heightSpin_->setValue(2048);
        dpiSpin_->setValue(72);
        layerCountSpin_->setValue(2);
        backgroundColor_ = QColor(128, 128, 128);
        transparentCheck_->setChecked(false);
    } else if (name == QStringLiteral("Pixel Art")) {
        widthSpin_->setValue(512);
        heightSpin_->setValue(512);
        dpiSpin_->setValue(72);
        layerCountSpin_->setValue(1);
        backgroundColor_ = Qt::transparent;
        transparentCheck_->setChecked(true);
    } else if (name == QStringLiteral("Default")) {
        widthSpin_->setValue(1920);
        heightSpin_->setValue(1080);
        dpiSpin_->setValue(300);
        layerCountSpin_->setValue(2);
        backgroundColor_ = Qt::white;
        transparentCheck_->setChecked(false);
    }
    backgroundButton_->setText(backgroundColor_.name(QColor::HexArgb));
}

void CreateOpenHub::selectTemplateItem(QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    const QString name = item->data(Qt::UserRole).toString();
    if (name.isEmpty()) {
        return;
    }

    const int index = templateCombo_->findText(name);
    if (index >= 0 && templateCombo_->currentIndex() != index) {
        templateCombo_->setCurrentIndex(index);
    } else {
        applyTemplate(name);
    }
}

QString CreateOpenHub::selectedRecentProjectPath() const
{
    if (!recentProjectsList_ || !recentProjectsList_->currentItem()) {
        return {};
    }
    return recentProjectsList_->currentItem()->data(Qt::UserRole).toString();
}

void CreateOpenHub::chooseOpenFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Project or Image"),
        QString(),
        QStringLiteral("HXP Project (*.hxp);;Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*.*)"));
    if (!path.isEmpty()) {
        selectedFilePath_ = path;
        openPathEdit_->setText(path);
    }
}

void CreateOpenHub::chooseImportFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Image"),
        QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*.*)"));
    if (!path.isEmpty()) {
        selectedFilePath_ = path;
        importPathEdit_->setText(path);
    }
}

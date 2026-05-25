#include "ExportDialog.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ExportDialog::ExportDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Export Image"));
    resize(520, 420);

    pathEdit_ = new QLineEdit(this);
    QPushButton *browse = new QPushButton(QStringLiteral("Browse..."), this);
    formatCombo_ = new QComboBox(this);
    formatCombo_->addItems({QStringLiteral("PNG"), QStringLiteral("JPG")});
    preserveTransparency_ = new QCheckBox(QStringLiteral("Preserve transparency"), this);
    preserveTransparency_->setChecked(true);
    mergeLayers_ = new QCheckBox(QStringLiteral("Merge visible layers"), this);
    mergeLayers_->setChecked(true);
    includeBackground_ = new QCheckBox(QStringLiteral("Include background"), this);
    includeBackground_->setChecked(true);
    qualitySpin_ = new QSpinBox(this);
    qualitySpin_->setRange(1, 100);
    qualitySpin_->setValue(92);
    resizeCheck_ = new QCheckBox(QStringLiteral("Resize on export"), this);
    resizeWidth_ = new QSpinBox(this);
    resizeWidth_->setRange(1, 32768);
    resizeWidth_->setValue(1920);
    resizeHeight_ = new QSpinBox(this);
    resizeHeight_->setRange(1, 32768);
    resizeHeight_->setValue(1080);
    metadataCheck_ = new QCheckBox(QStringLiteral("Include metadata"), this);
    previewLabel_ = new QLabel(QStringLiteral("Export uses the current composited image and selected format settings."), this);
    previewLabel_->setWordWrap(true);

    QWidget *pathRow = new QWidget(this);
    QHBoxLayout *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->addWidget(pathEdit_);
    pathLayout->addWidget(browse);

    QFormLayout *form = new QFormLayout;
    form->addRow(QStringLiteral("File"), pathRow);
    form->addRow(QStringLiteral("Format"), formatCombo_);
    form->addRow(QString(), preserveTransparency_);
    form->addRow(QString(), mergeLayers_);
    form->addRow(QString(), includeBackground_);
    form->addRow(QStringLiteral("Quality"), qualitySpin_);
    form->addRow(QString(), resizeCheck_);
    form->addRow(QStringLiteral("Width"), resizeWidth_);
    form->addRow(QStringLiteral("Height"), resizeHeight_);
    form->addRow(QString(), metadataCheck_);
    form->addRow(QStringLiteral("Preview"), previewLabel_);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(browse, &QPushButton::clicked, this, &ExportDialog::chooseFile);
    QObject::connect(formatCombo_, &QComboBox::currentTextChanged, this, [this] {
        if (!pathEdit_->text().trimmed().isEmpty()) {
            QFileInfo info(pathEdit_->text().trimmed());
            const QString suffix = formatCombo_->currentText().compare(QStringLiteral("JPG"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("jpg")
                : QStringLiteral("png");
            pathEdit_->setText(info.path() + QStringLiteral("/") + info.completeBaseName() + QStringLiteral(".") + suffix);
        }
        updatePreviewText();
    });
    QObject::connect(pathEdit_, &QLineEdit::textChanged, this, &ExportDialog::updatePreviewText);
    QObject::connect(preserveTransparency_, &QCheckBox::toggled, this, &ExportDialog::updatePreviewText);
    QObject::connect(includeBackground_, &QCheckBox::toggled, this, &ExportDialog::updatePreviewText);
    QObject::connect(resizeCheck_, &QCheckBox::toggled, this, &ExportDialog::updateResizeControls);
    QObject::connect(resizeWidth_, &QSpinBox::valueChanged, this, &ExportDialog::updatePreviewText);
    QObject::connect(resizeHeight_, &QSpinBox::valueChanged, this, &ExportDialog::updatePreviewText);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    updateResizeControls();
    updatePreviewText();
}

void ExportDialog::setInitialPath(const QString &path)
{
    pathEdit_->setText(path);
    updateFormatFromPath();
}

ExportOptions ExportDialog::options() const
{
    ExportOptions result;
    result.filePath = pathEdit_->text();
    result.format = formatCombo_->currentText();
    result.preserveTransparency = preserveTransparency_->isChecked();
    result.mergeLayers = mergeLayers_->isChecked();
    result.includeBackground = includeBackground_->isChecked();
    result.resizeEnabled = resizeCheck_->isChecked();
    result.targetSize = QSize(resizeWidth_->value(), resizeHeight_->value());
    result.includeMetadata = metadataCheck_->isChecked();
    result.quality = qualitySpin_->value();

    const QString suffix = result.format.compare(QStringLiteral("JPG"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("jpg")
        : QStringLiteral("png");
    if (!result.filePath.trimmed().isEmpty()) {
        QFileInfo info(result.filePath);
        if (info.suffix().compare(suffix, Qt::CaseInsensitive) != 0) {
            const QString baseName = info.completeBaseName().isEmpty() ? QStringLiteral("hxpainter-export") : info.completeBaseName();
            const QString dir = info.path().isEmpty() || info.path() == QStringLiteral(".") ? QString() : info.path() + QStringLiteral("/");
            result.filePath = dir + baseName + QStringLiteral(".") + suffix;
        }
    }
    return result;
}

void ExportDialog::chooseFile()
{
    QString initialPath = pathEdit_->text().trimmed();
    if (initialPath.isEmpty()) {
        initialPath = QStringLiteral("hxpainter-export.png");
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Image"),
        initialPath,
        QStringLiteral("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)"));
    if (!path.isEmpty()) {
        pathEdit_->setText(path);
        updateFormatFromPath();
    }
}

void ExportDialog::updateFormatFromPath()
{
    const QString lower = pathEdit_->text().toLower();
    if (lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg"))) {
        formatCombo_->setCurrentText(QStringLiteral("JPG"));
        preserveTransparency_->setChecked(false);
    } else {
        formatCombo_->setCurrentText(QStringLiteral("PNG"));
    }
    updatePreviewText();
}

void ExportDialog::updateResizeControls()
{
    const bool enabled = resizeCheck_->isChecked();
    resizeWidth_->setEnabled(enabled);
    resizeHeight_->setEnabled(enabled);
    updatePreviewText();
}

void ExportDialog::updatePreviewText()
{
    const QString transparency = formatCombo_->currentText() == QStringLiteral("JPG")
        ? QStringLiteral("JPG flattens transparency to the matte color.")
        : (preserveTransparency_->isChecked() ? QStringLiteral("PNG keeps transparency.") : QStringLiteral("PNG will be flattened."));
    const QString resize = resizeCheck_->isChecked()
        ? QStringLiteral(" Resize: %1 x %2.").arg(resizeWidth_->value()).arg(resizeHeight_->value())
        : QStringLiteral(" Original canvas size.");
    previewLabel_->setText(transparency + resize);
}

#include "TextSettingsDialog.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

TextSettingsDialog::TextSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Text Settings"));
    setModal(true);

    textEdit_ = new QPlainTextEdit(this);
    textEdit_->setPlaceholderText(QStringLiteral("Text"));
    textEdit_->setMinimumHeight(120);

    fontCombo_ = new QFontComboBox(this);
    pointSize_ = new QSpinBox(this);
    pointSize_->setRange(4, 256);
    pointSize_->setValue(32);
    boxWidth_ = new QSpinBox(this);
    boxWidth_->setRange(32, 8192);
    boxWidth_->setValue(480);
    boxHeight_ = new QSpinBox(this);
    boxHeight_->setRange(32, 8192);
    boxHeight_->setValue(220);
    alpha_ = new QSpinBox(this);
    alpha_->setRange(0, 255);
    alpha_->setValue(255);

    colorButton_ = new QPushButton(this);
    QObject::connect(colorButton_, &QPushButton::clicked, this, &TextSettingsDialog::chooseColor);
    QObject::connect(alpha_, &QSpinBox::valueChanged, this, &TextSettingsDialog::updateColorButton);

    QFormLayout *form = new QFormLayout;
    form->addRow(QStringLiteral("Font"), fontCombo_);
    form->addRow(QStringLiteral("Size"), pointSize_);
    form->addRow(QStringLiteral("Color"), colorButton_);
    form->addRow(QStringLiteral("Alpha"), alpha_);
    form->addRow(QStringLiteral("Box Width"), boxWidth_);
    form->addRow(QStringLiteral("Box Height"), boxHeight_);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &TextSettingsDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &TextSettingsDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(textEdit_);
    layout->addLayout(form);
    layout->addWidget(buttons);

    updateColorButton();
}

void TextSettingsDialog::setSettings(const TextSettings &settings)
{
    if (!settings.font.family().isEmpty()) {
        fontCombo_->setCurrentFont(settings.font);
    }
    if (settings.font.pointSize() > 0) {
        pointSize_->setValue(settings.font.pointSize());
    }
    if (settings.boxSize.width() >= 32.0) {
        boxWidth_->setValue(static_cast<int>(settings.boxSize.width()));
    }
    if (settings.boxSize.height() >= 32.0) {
        boxHeight_->setValue(static_cast<int>(settings.boxSize.height()));
    }
    color_ = settings.color.isValid() ? settings.color : QColor(Qt::black);
    alpha_->setValue(color_.alpha());
    updateColorButton();
}

TextSettings TextSettingsDialog::settings() const
{
    TextSettings result;
    QFont font = fontCombo_->currentFont();
    font.setPointSize(pointSize_->value());
    result.font = font;

    QColor textColor = color_;
    textColor.setAlpha(alpha_->value());
    result.color = textColor;
    result.boxSize = QSizeF(boxWidth_->value(), boxHeight_->value());
    return result;
}

QString TextSettingsDialog::text() const
{
    return textEdit_->toPlainText();
}

void TextSettingsDialog::accept()
{
    if (text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Text Settings"), QStringLiteral("Enter text before inserting."));
        return;
    }
    QDialog::accept();
}

void TextSettingsDialog::chooseColor()
{
    QColor selected = color_;
    selected.setAlpha(alpha_->value());
    selected = QColorDialog::getColor(selected, this, QStringLiteral("Text Color"), QColorDialog::ShowAlphaChannel);
    if (!selected.isValid()) {
        return;
    }
    color_ = selected;
    alpha_->setValue(selected.alpha());
    updateColorButton();
}

void TextSettingsDialog::updateColorButton()
{
    QColor preview = color_;
    preview.setAlpha(alpha_ ? alpha_->value() : color_.alpha());
    const int luma = (preview.red() * 299 + preview.green() * 587 + preview.blue() * 114) / 1000;
    const QString textColor = luma > 150 && preview.alpha() > 180 ? QStringLiteral("#11151c") : QStringLiteral("#f6f8fb");
    colorButton_->setText(preview.name(QColor::HexArgb));
    colorButton_->setStyleSheet(QStringLiteral("background:%1;color:%2;").arg(preview.name(QColor::HexArgb), textColor));
}

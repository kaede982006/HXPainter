#pragma once

#include "brush/TextEngine.h"

#include <QDialog>

class QFontComboBox;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class TextSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit TextSettingsDialog(QWidget *parent = nullptr);

    void setSettings(const TextSettings &settings);
    [[nodiscard]] TextSettings settings() const;
    [[nodiscard]] QString text() const;

protected:
    void accept() override;

private:
    void chooseColor();
    void updateColorButton();

    QPlainTextEdit *textEdit_ = nullptr;
    QFontComboBox *fontCombo_ = nullptr;
    QSpinBox *pointSize_ = nullptr;
    QSpinBox *boxWidth_ = nullptr;
    QSpinBox *boxHeight_ = nullptr;
    QSpinBox *alpha_ = nullptr;
    QPushButton *colorButton_ = nullptr;
    QColor color_ = Qt::black;
};

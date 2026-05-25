#pragma once

#include "export/ExportManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>

class ExportDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(QWidget *parent = nullptr);
    void setInitialPath(const QString &path);
    [[nodiscard]] ExportOptions options() const;

private:
    void chooseFile();
    void updateFormatFromPath();
    void updateResizeControls();
    void updatePreviewText();

    QLineEdit *pathEdit_ = nullptr;
    QComboBox *formatCombo_ = nullptr;
    QCheckBox *preserveTransparency_ = nullptr;
    QCheckBox *mergeLayers_ = nullptr;
    QCheckBox *includeBackground_ = nullptr;
    QSpinBox *qualitySpin_ = nullptr;
    QCheckBox *resizeCheck_ = nullptr;
    QSpinBox *resizeWidth_ = nullptr;
    QSpinBox *resizeHeight_ = nullptr;
    QCheckBox *metadataCheck_ = nullptr;
    QLabel *previewLabel_ = nullptr;
};

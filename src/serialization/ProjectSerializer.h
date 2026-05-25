#pragma once

#include "render/CanvasDocument.h"

#include <QString>

class ProjectSerializer {
public:
    static bool save(const CanvasDocument &document, const QString &filePath, QString *error = nullptr);
    static bool load(CanvasDocument &document, const QString &filePath, QString *error = nullptr);

private:
    static QString imageToBase64Png(const QImage &image);
    static QImage imageFromBase64Png(const QString &encoded);
};

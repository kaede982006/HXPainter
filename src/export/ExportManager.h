#pragma once

#include "render/CanvasDocument.h"

#include <QColor>
#include <QSize>
#include <QString>

struct ExportOptions {
    QString filePath;
    QString format = QStringLiteral("PNG");
    bool preserveTransparency = true;
    bool mergeLayers = true;
    bool includeBackground = true;
    bool resizeEnabled = false;
    QSize targetSize;
    bool includeMetadata = false;
    int quality = 92;
    QColor matteColor = Qt::white;
};

class ExportManager {
public:
    static bool exportDocument(const CanvasDocument &document, const ExportOptions &options, QString *error = nullptr);
};

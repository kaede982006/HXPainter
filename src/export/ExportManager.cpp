#include "ExportManager.h"

#include <QFileInfo>
#include <QPainter>

#include <algorithm>

bool ExportManager::exportDocument(const CanvasDocument &document, const ExportOptions &options, QString *error)
{
    if (!document.isValid()) {
        if (error) {
            *error = QStringLiteral("No document to export.");
        }
        return false;
    }

    QImage image;
    if (options.mergeLayers) {
        image = document.compositedImage(options.includeBackground);
    } else if (const Layer *layer = document.activeLayer()) {
        image = QImage(document.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(options.includeBackground ? options.matteColor : QColor(Qt::transparent));
        if (layer->visible && !layer->image.isNull()) {
            QPainter painter(&image);
            painter.setOpacity(std::clamp(layer->opacity, 0.0, 1.0));
            painter.drawImage(QPoint(0, 0), layer->image);
        }
    }
    if (image.isNull()) {
        if (error) {
            *error = QStringLiteral("Failed to prepare export image.");
        }
        return false;
    }

    QString format = options.format.trimmed().toLower();
    if (format == QStringLiteral("jpeg")) {
        format = QStringLiteral("jpg");
    }
    if (format.isEmpty()) {
        format = QFileInfo(options.filePath).suffix().toLower();
    }
    if (format.isEmpty()) {
        format = QStringLiteral("png");
    }

    if (options.resizeEnabled && options.targetSize.isValid()) {
        image = image.scaled(options.targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (options.includeMetadata) {
        image.setText(QStringLiteral("Software"), QStringLiteral("HXPainter"));
        image.setText(QStringLiteral("CanvasSize"), QStringLiteral("%1x%2").arg(document.size().width()).arg(document.size().height()));
    }

    if (format == QStringLiteral("jpg") || format == QStringLiteral("jpeg")) {
        QImage flattened(image.size(), QImage::Format_RGB32);
        flattened.fill(options.matteColor);
        QPainter painter(&flattened);
        painter.drawImage(QPoint(0, 0), image);
        if (options.includeMetadata) {
            flattened.setText(QStringLiteral("Software"), QStringLiteral("HXPainter"));
        }
        if (!flattened.save(options.filePath, "JPG", options.quality)) {
            if (error) {
                *error = QStringLiteral("Failed to export JPG.");
            }
            return false;
        }
        return true;
    }

    if (!options.preserveTransparency) {
        QImage flattened(image.size(), QImage::Format_RGB32);
        flattened.fill(options.matteColor);
        QPainter painter(&flattened);
        painter.drawImage(QPoint(0, 0), image);
        image = flattened;
    }

    const QByteArray fmt = format.toUpper().toLatin1();
    if (!image.save(options.filePath, fmt.constData(), options.quality)) {
        if (error) {
            *error = QStringLiteral("Failed to export image.");
        }
        return false;
    }
    return true;
}

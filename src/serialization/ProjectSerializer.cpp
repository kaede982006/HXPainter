#include "ProjectSerializer.h"

#include <QBuffer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <algorithm>

bool ProjectSerializer::save(const CanvasDocument &document, const QString &filePath, QString *error)
{
    if (!document.isValid()) {
        if (error) {
            *error = QStringLiteral("No document to save.");
        }
        return false;
    }
    if (filePath.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Save path is empty.");
        }
        return false;
    }

    QJsonObject root;
    root["version"] = 1;
    root["width"] = document.size().width();
    root["height"] = document.size().height();
    root["activeLayerIndex"] = document.activeLayerIndex();
    const DocumentState snapshot = document.snapshot();
    root["dpi"] = snapshot.dpi;
    root["backgroundColor"] = snapshot.backgroundColor.name(QColor::HexArgb);
    root["transparentBackground"] = snapshot.transparentBackground;
    root["colorSpace"] = snapshot.colorSpace;
    root["bitDepth"] = snapshot.bitDepth;

    QJsonArray layers;
    for (const Layer &layer : document.layers()) {
        QJsonObject item;
        item["id"] = layer.id;
        item["name"] = layer.name;
        item["visible"] = layer.visible;
        item["locked"] = layer.locked;
        item["alphaLock"] = layer.alphaLock;
        item["opacity"] = layer.opacity;
        item["blendMode"] = layer.blendMode;
        const QString encodedImage = imageToBase64Png(layer.image);
        if (encodedImage.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Could not encode layer image: %1").arg(layer.name);
            }
            return false;
        }
        item["image"] = encodedImage;
        layers.push_back(item);
    }
    root["layers"] = layers;

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const qint64 written = file.write(bytes);
    if (written != bytes.size()) {
        if (error) {
            *error = QStringLiteral("Could not write complete project file: %1").arg(file.errorString());
        }
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("Could not commit project file: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool ProjectSerializer::load(CanvasDocument &document, const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!json.isObject()) {
        if (error) {
            *error = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Invalid HXP project file.")
                : QStringLiteral("Invalid HXP project JSON at offset %1: %2")
                    .arg(parseError.offset)
                    .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = json.object();
    DocumentState state;
    state.canvasSize = QSize(root["width"].toInt(), root["height"].toInt());
    if (state.canvasSize.width() <= 0 || state.canvasSize.height() <= 0) {
        if (error) {
            *error = QStringLiteral("Project canvas size is invalid.");
        }
        return false;
    }
    state.activeLayerIndex = root["activeLayerIndex"].toInt(0);
    state.dpi = root["dpi"].toInt(300);
    state.backgroundColor = QColor(root["backgroundColor"].toString(QStringLiteral("#ffffffff")));
    if (!state.backgroundColor.isValid()) {
        state.backgroundColor = Qt::white;
    }
    state.transparentBackground = root["transparentBackground"].toBool(false);
    state.colorSpace = root["colorSpace"].toString(QStringLiteral("sRGB"));
    state.bitDepth = root["bitDepth"].toInt(8);
    state.filePath = filePath;
    state.modified = false;

    if (!root["layers"].isArray()) {
        if (error) {
            *error = QStringLiteral("Project has no layer array.");
        }
        return false;
    }
    const QJsonArray layers = root["layers"].toArray();
    int layerNumber = 0;
    for (const QJsonValue &value : layers) {
        ++layerNumber;
        if (!value.isObject()) {
            if (error) {
                *error = QStringLiteral("Layer %1 is not an object.").arg(layerNumber);
            }
            return false;
        }
        const QJsonObject item = value.toObject();
        Layer layer;
        layer.id = item["id"].toString();
        layer.name = item["name"].toString(QStringLiteral("Layer"));
        layer.visible = item["visible"].toBool(true);
        layer.locked = item["locked"].toBool(false);
        layer.alphaLock = item["alphaLock"].toBool(false);
        layer.opacity = std::clamp(item["opacity"].toDouble(1.0), 0.0, 1.0);
        layer.blendMode = item["blendMode"].toString(QStringLiteral("Normal"));
        layer.image = imageFromBase64Png(item["image"].toString()).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if (layer.image.isNull()) {
            if (error) {
                *error = QStringLiteral("Layer %1 image is missing or invalid.").arg(layerNumber);
            }
            return false;
        }
        if (layer.image.size() != state.canvasSize) {
            if (error) {
                *error = QStringLiteral("Layer %1 size %2x%3 does not match canvas %4x%5.")
                    .arg(layerNumber)
                    .arg(layer.image.width())
                    .arg(layer.image.height())
                    .arg(state.canvasSize.width())
                    .arg(state.canvasSize.height());
            }
            return false;
        }
        state.layers.push_back(layer);
    }

    if (state.canvasSize.isEmpty() || state.layers.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Project has no layers.");
        }
        return false;
    }
    if (state.activeLayerIndex < 0 || state.activeLayerIndex >= state.layers.size()) {
        state.activeLayerIndex = state.layers.size() - 1;
    }

    document.restore(state);
    return true;
}

QString ProjectSerializer::imageToBase64Png(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        return {};
    }
    return QString::fromLatin1(bytes.toBase64());
}

QImage ProjectSerializer::imageFromBase64Png(const QString &encoded)
{
    const QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1());
    QImage image;
    image.loadFromData(bytes, "PNG");
    return image;
}

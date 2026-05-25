#pragma once

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>

struct Layer {
    QString id;
    QString name;
    QImage image;
    bool visible = true;
    bool locked = false;
    bool alphaLock = false;
    double opacity = 1.0;
    QString blendMode = QStringLiteral("Normal");

    [[nodiscard]] QImage thumbnail(int size = 48) const;
};

struct NewDocumentSettings {
    int width = 1920;
    int height = 1080;
    int dpi = 300;
    QColor backgroundColor = Qt::white;
    bool transparentBackground = false;
    QString colorSpace = QStringLiteral("sRGB");
    int bitDepth = 8;
    int defaultLayerCount = 2;
    QString templateName = QStringLiteral("Default");
};

struct DocumentState {
    QSize canvasSize;
    int dpi = 300;
    QColor backgroundColor = Qt::white;
    bool transparentBackground = false;
    QString colorSpace = QStringLiteral("sRGB");
    int bitDepth = 8;
    QVector<Layer> layers;
    int activeLayerIndex = -1;
    QString filePath;
    bool modified = false;
};

class CanvasDocument {
public:
    CanvasDocument() = default;

    void reset(const NewDocumentSettings &settings);
    void reset(const QSize &size, const QColor &background = QColor(Qt::white));
    bool loadImageAsDocument(const QString &filePath);
    bool importImageAsLayer(const QString &filePath, QString *error = nullptr);
    bool addImageLayer(const QImage &image, const QString &name, QString *error = nullptr, bool centered = true);
    bool exportCompositedImage(const QString &filePath, const QColor &matte = Qt::white) const;

    [[nodiscard]] QSize size() const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isModified() const;
    [[nodiscard]] QString filePath() const;
    [[nodiscard]] int activeLayerIndex() const;
    [[nodiscard]] const QVector<Layer> &layers() const;
    [[nodiscard]] QVector<Layer> &layers();
    [[nodiscard]] const Layer *activeLayer() const;
    Layer *activeLayer();
    [[nodiscard]] QImage *activeImage();
    [[nodiscard]] const QImage *activeImage() const;
    [[nodiscard]] QImage compositedImage(bool includeBackground = true) const;
    [[nodiscard]] DocumentState snapshot() const;

    void restore(const DocumentState &state);
    void setFilePath(const QString &filePath);
    void setModified(bool modified);
    void setActiveLayerIndex(int index);

    int addLayer(const QString &name = QString());
    bool deleteLayer(int index);
    int duplicateLayer(int index);
    bool moveLayer(int from, int to);
    bool mergeLayerDown(int index);
    bool renameLayer(int index, const QString &name);
    bool setLayerVisible(int index, bool visible);
    bool setLayerLocked(int index, bool locked);
    bool setLayerAlphaLock(int index, bool alphaLock);
    bool setLayerOpacity(int index, double opacity);
    bool setLayerBlendMode(int index, const QString &blendMode);
    bool clearLayer(int index, const QRect &rect = QRect());

private:
    static Layer makeLayer(const QSize &size, const QString &name, const QColor &fill = Qt::transparent);
    [[nodiscard]] bool validLayerIndex(int index) const;
    void invalidateCompositeCache() const;

    DocumentState state_;
    mutable bool compositeWithBackgroundValid_ = false;
    mutable bool compositeWithoutBackgroundValid_ = false;
    mutable QImage compositeWithBackground_;
    mutable QImage compositeWithoutBackground_;
};

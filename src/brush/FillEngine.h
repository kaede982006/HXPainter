#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QVector>

struct FillSettings {
    int tolerance = 24;
    bool contiguous = true;
    bool sampleAllLayers = false;
    double opacity = 1.0;
};

struct FillResult {
    QRect dirtyRect;
    int pixelsChanged = 0;

    [[nodiscard]] bool hasChanges() const
    {
        return pixelsChanged > 0 && dirtyRect.isValid();
    }
};

struct FillRegion {
    QRect dirtyRect;
    QVector<int> pixelIndexes;

    [[nodiscard]] bool hasPixels() const
    {
        return !pixelIndexes.isEmpty() && dirtyRect.isValid();
    }
};

class FillEngine {
public:
    static FillRegion matchedRegion(const QImage &source, const QPoint &start, const FillSettings &settings);
    static FillResult fillRegion(QImage &target, const FillRegion &region, const QColor &color, const FillSettings &settings);
    static FillResult floodFill(QImage &target, const QPoint &start, const QColor &color, const FillSettings &settings);

private:
    static QRgb destinationOver(QRgb destination, QRgb source);
    static bool withinTolerance(QRgb lhs, QRgb rhs, int tolerance);
};

#include "FillEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <QPair>
#include <utility>

namespace {
QColor colorWithOpacity(const QColor &source, double opacity)
{
    QColor color = source;
    color.setAlphaF(std::clamp(opacity, 0.0, 1.0) * source.alphaF());
    return color;
}
}

FillRegion FillEngine::matchedRegion(const QImage &source, const QPoint &start, const FillSettings &settings)
{
    if (source.isNull() || !QRect(QPoint(0, 0), source.size()).contains(start)) {
        return {};
    }

    const QImage working = source.convertToFormat(QImage::Format_ARGB32);
    const QRgb seed = working.pixel(start);
    const int tolerance = std::clamp(settings.tolerance, 0, 255);
    const int width = working.width();
    const int height = working.height();

    FillRegion region;
    auto addPixel = [&](int x, int y) {
        region.pixelIndexes.push_back(y * width + x);
        const QRect pixelRect(x, y, 1, 1);
        region.dirtyRect = region.dirtyRect.isValid() ? region.dirtyRect.united(pixelRect) : pixelRect;
    };

    if (!settings.contiguous) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (withinTolerance(working.pixel(x, y), seed, tolerance)) {
                    addPixel(x, y);
                }
            }
        }
    } else {
        QVector<uchar> visited(width * height, 0);
        QVector<QPoint> queue;
        queue.reserve(1024);
        queue.push_back(start);
        visited[start.y() * width + start.x()] = 1;

        for (int head = 0; head < queue.size(); ++head) {
            const QPoint p = queue[head];
            if (!withinTolerance(working.pixel(p), seed, tolerance)) {
                continue;
            }

            addPixel(p.x(), p.y());

            const QPoint neighbors[] = {
                QPoint(p.x() + 1, p.y()),
                QPoint(p.x() - 1, p.y()),
                QPoint(p.x(), p.y() + 1),
                QPoint(p.x(), p.y() - 1),
            };

            for (const QPoint &next : neighbors) {
                if (next.x() < 0 || next.y() < 0 || next.x() >= width || next.y() >= height) {
                    continue;
                }
                const int index = next.y() * width + next.x();
                if (visited[index]) {
                    continue;
                }
                visited[index] = 1;
                queue.push_back(next);
            }
        }
    }

    return region;
}

FillResult FillEngine::fillRegion(QImage &target, const FillRegion &region, const QColor &color, const FillSettings &settings)
{
    if (target.isNull() || !region.hasPixels()) {
        return {};
    }

    QImage working = target.convertToFormat(QImage::Format_ARGB32);
    const QImage original = working;
    const QColor fillColor = colorWithOpacity(color, settings.opacity);
    const QRgb replacement = fillColor.rgba();

    QRect dirty;
    int changed = 0;
    const int width = working.width();
    const int height = working.height();
    QVector<uchar> regionMask(width * height, 0);
    for (int index : std::as_const(region.pixelIndexes)) {
        if (index >= 0 && index < regionMask.size()) {
            regionMask[index] = 1;
        }
    }

    for (int index : std::as_const(region.pixelIndexes)) {
        const int x = index % width;
        const int y = index / width;
        if (working.pixel(x, y) == replacement) {
            continue;
        }
        working.setPixel(x, y, replacement);
        const QRect pixelRect(x, y, 1, 1);
        dirty = dirty.isValid() ? dirty.united(pixelRect) : pixelRect;
        ++changed;
    }

    QVector<uchar> fringeVisited(width * height, 0);
    QVector<QPair<int, int>> fringeQueue;
    fringeQueue.reserve(region.pixelIndexes.size());
    for (int index : std::as_const(region.pixelIndexes)) {
        if (index >= 0 && index < fringeVisited.size()) {
            fringeVisited[index] = 1;
            fringeQueue.push_back({index, 0});
        }
    }

    const int maxUnderpaintDepth = 2;
    for (int head = 0; head < fringeQueue.size(); ++head) {
        const int index = fringeQueue[head].first;
        const int depth = fringeQueue[head].second;
        if (depth >= maxUnderpaintDepth) {
            continue;
        }
        const int x = index % width;
        const int y = index / width;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                    continue;
                }
                const int neighborIndex = ny * width + nx;
                if (regionMask[neighborIndex] || fringeVisited[neighborIndex]) {
                    continue;
                }
                fringeVisited[neighborIndex] = 1;

                const QRgb originalPixel = original.pixel(nx, ny);
                const int alpha = qAlpha(originalPixel);
                if (alpha <= 0 || alpha >= 255) {
                    continue;
                }

                const QRgb composited = destinationOver(originalPixel, replacement);
                if (composited == working.pixel(nx, ny)) {
                    continue;
                }
                working.setPixel(nx, ny, composited);
                const QRect pixelRect(nx, ny, 1, 1);
                dirty = dirty.isValid() ? dirty.united(pixelRect) : pixelRect;
                ++changed;
                fringeQueue.push_back({neighborIndex, depth + 1});
            }
        }
    }

    if (changed > 0) {
        target = working.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    return {dirty, changed};
}

FillResult FillEngine::floodFill(QImage &target, const QPoint &start, const QColor &color, const FillSettings &settings)
{
    if (target.isNull() || !QRect(QPoint(0, 0), target.size()).contains(start)) {
        return {};
    }

    const FillRegion region = matchedRegion(target, start, settings);
    return fillRegion(target, region, color, settings);
}

QRgb FillEngine::destinationOver(QRgb destination, QRgb source)
{
    const double da = qAlpha(destination) / 255.0;
    const double sa = qAlpha(source) / 255.0;
    const double outA = da + sa * (1.0 - da);
    if (outA <= 0.0) {
        return qRgba(0, 0, 0, 0);
    }

    const auto channel = [da, sa, outA](int d, int s) {
        const double value = (d * da + s * sa * (1.0 - da)) / outA;
        return std::clamp(static_cast<int>(std::round(value)), 0, 255);
    };

    const int alpha = std::clamp(static_cast<int>(std::round(outA * 255.0)), 0, 255);
    return qRgba(channel(qRed(destination), qRed(source)),
                 channel(qGreen(destination), qGreen(source)),
                 channel(qBlue(destination), qBlue(source)),
                 alpha);
}

bool FillEngine::withinTolerance(QRgb lhs, QRgb rhs, int tolerance)
{
    const int dr = qRed(lhs) - qRed(rhs);
    const int dg = qGreen(lhs) - qGreen(rhs);
    const int db = qBlue(lhs) - qBlue(rhs);
    const int da = qAlpha(lhs) - qAlpha(rhs);
    const int channelTolerance = std::clamp(tolerance, 0, 255);
    return std::abs(dr) <= channelTolerance
        && std::abs(dg) <= channelTolerance
        && std::abs(db) <= channelTolerance
        && std::abs(da) <= channelTolerance;
}

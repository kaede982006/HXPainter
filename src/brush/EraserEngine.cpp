#include "EraserEngine.h"

#include <QPainter>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

EraserRenderResult EraserEngine::beginStroke(QImage &target, const StrokeSample &sample, const EraserSettings &settings)
{
    lastSample_ = sample;
    const QRect dirty = stamp(target, sample, settings);
    return {dirty, dirty.isValid() ? 1 : 0};
}

EraserRenderResult EraserEngine::continueStroke(QImage &target, const StrokeSample &sample, const EraserSettings &settings)
{
    if (!lastSample_.has_value()) {
        return beginStroke(target, sample, settings);
    }

    const StrokeSample previous = *lastSample_;
    const QPointF delta = sample.position - previous.position;
    const double distance = std::hypot(delta.x(), delta.y());
    const double radius = std::max(1.0, effectiveRadius(settings, sample.pressure));
    const double step = std::max(1.0, radius * std::clamp(settings.spacing, 0.05, 1.0));
    const int count = std::max(1, static_cast<int>(std::ceil(distance / step)));

    QRect dirty;
    int samples = 0;
    for (int i = 1; i <= count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(count);
        StrokeSample interpolated;
        interpolated.position = previous.position + delta * t;
        interpolated.pressure = previous.pressure + (sample.pressure - previous.pressure) * t;
        interpolated.timestampNs = sample.timestampNs;

        const QRect stampDirty = stamp(target, interpolated, settings);
        if (stampDirty.isValid()) {
            dirty = dirty.isValid() ? dirty.united(stampDirty) : stampDirty;
            ++samples;
        }
    }

    lastSample_ = sample;
    return {dirty, samples};
}

void EraserEngine::endStroke()
{
    lastSample_.reset();
}

double EraserEngine::effectiveRadius(const EraserSettings &settings, double pressure)
{
    if (!settings.pressureControlsRadius) {
        return settings.radius;
    }
    return settings.radius * (0.25 + std::clamp(pressure, 0.0, 1.0) * 0.75);
}

QRect EraserEngine::stamp(QImage &target, const StrokeSample &sample, const EraserSettings &settings)
{
    if (target.isNull()) {
        return {};
    }

    const double radius = std::max(0.5, effectiveRadius(settings, sample.pressure));
    const QRectF bounds(sample.position.x() - radius,
                        sample.position.y() - radius,
                        radius * 2.0,
                        radius * 2.0);
    const QRect imageBounds(QPoint(0, 0), target.size());
    const QRect dirty = bounds.toAlignedRect().adjusted(-1, -1, 1, 1).intersected(imageBounds);
    if (!dirty.isValid()) {
        return {};
    }
    const QImage before = target.copy(dirty);

    QRadialGradient gradient(sample.position, radius);
    gradient.setColorAt(0.0, QColor(0, 0, 0, static_cast<int>(std::clamp(settings.opacity, 0.0, 1.0) * 255.0)));
    gradient.setColorAt(1.0, QColor(0, 0, 0, 0));

    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(bounds);
    painter.end();

    if (target.copy(dirty) == before) {
        return {};
    }

    return dirty;
}

#include "BrushEngine.h"

#include <QPainter>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

BrushRenderResult BrushEngine::beginStroke(QImage &target, const StrokeSample &sample, const BrushSettings &settings)
{
    lastSample_ = sample;
    const QRect dirty = stamp(target, sample, settings);
    return {dirty, dirty.isValid() ? 1 : 0};
}

BrushRenderResult BrushEngine::continueStroke(QImage &target, const StrokeSample &sample, const BrushSettings &settings)
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

void BrushEngine::endStroke()
{
    lastSample_.reset();
}

double BrushEngine::effectiveRadius(const BrushSettings &settings, double pressure)
{
    const double normalizedPressure = std::clamp(pressure, 0.0, 1.0);
    if (!settings.pressureControlsRadius) {
        return settings.radius;
    }
    return settings.radius * (0.25 + normalizedPressure * 0.75);
}

double BrushEngine::effectiveOpacity(const BrushSettings &settings, double pressure)
{
    const double baseOpacity = std::clamp(settings.opacity, 0.0, 1.0)
        * std::clamp(settings.flow, 0.01, 1.0);
    if (!settings.pressureControlsOpacity) {
        return baseOpacity;
    }
    return baseOpacity * std::clamp(pressure, 0.05, 1.0);
}

QRect BrushEngine::stamp(QImage &target, const StrokeSample &sample, const BrushSettings &settings)
{
    if (target.isNull()) {
        return {};
    }

    const double radius = std::max(0.5, effectiveRadius(settings, sample.pressure));
    const double opacity = effectiveOpacity(settings, sample.pressure);
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

    QColor core = settings.color;
    core.setAlphaF(opacity * settings.color.alphaF());

    QColor edge = settings.color;
    edge.setAlphaF(0.0);

    QRadialGradient gradient(sample.position, radius);
    gradient.setColorAt(0.0, core);
    gradient.setColorAt(std::clamp(settings.hardness, 0.0, 1.0), core);
    gradient.setColorAt(1.0, edge);

    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(bounds);
    painter.end();

    if (target.copy(dirty) == before) {
        return {};
    }

    return dirty;
}

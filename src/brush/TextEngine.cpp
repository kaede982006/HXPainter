#include "TextEngine.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>

TextRenderResult TextEngine::drawTextBox(QImage &target, const QPointF &position, const QString &text, const TextSettings &settings)
{
    if (target.isNull() || text.trimmed().isEmpty()) {
        return {};
    }

    const QRectF imageRect(QPointF(0, 0), QSizeF(target.size()));
    if (!imageRect.contains(position)) {
        return {};
    }

    const QFontMetricsF metrics(settings.font);
    const double minimumWidth = std::max(32.0, metrics.averageCharWidth() * 8.0);
    const double minimumHeight = std::max(32.0, metrics.height() * 1.5);
    const QSizeF requestedSize(
        std::max(minimumWidth, settings.boxSize.width()),
        std::max(minimumHeight, settings.boxSize.height()));
    const QSizeF boundedSize(
        std::max(minimumWidth, std::min(requestedSize.width(), imageRect.width() - position.x())),
        std::max(minimumHeight, std::min(requestedSize.height(), imageRect.height() - position.y())));
    const QRectF textRect(position, boundedSize);

    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setFont(settings.font);
    painter.setPen(settings.color);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);

    return {textRect.toAlignedRect().intersected(QRect(QPoint(0, 0), target.size())), true};
}

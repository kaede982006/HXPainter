#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QSizeF>
#include <QString>

struct TextSettings {
    QFont font;
    QColor color = Qt::black;
    QSizeF boxSize = QSizeF(480.0, 220.0);
};

struct TextRenderResult {
    QRect dirtyRect;
    bool changed = false;
};

class TextEngine {
public:
    static TextRenderResult drawTextBox(QImage &target, const QPointF &position, const QString &text, const TextSettings &settings);
};

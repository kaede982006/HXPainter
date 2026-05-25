#include "AppTheme.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QTextStream>
#include <QDebug>

void AppTheme::apply(QApplication &app)
{
    if (QStyle *fusionStyle = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusionStyle);
    }

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(23, 26, 32));
    palette.setColor(QPalette::WindowText, QColor(219, 228, 238));
    palette.setColor(QPalette::Base, QColor(17, 21, 28));
    palette.setColor(QPalette::AlternateBase, QColor(23, 28, 36));
    palette.setColor(QPalette::ToolTipBase, QColor(36, 43, 55));
    palette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    palette.setColor(QPalette::Text, QColor(219, 228, 238));
    palette.setColor(QPalette::Button, QColor(36, 43, 55));
    palette.setColor(QPalette::ButtonText, QColor(219, 228, 238));
    palette.setColor(QPalette::BrightText, QColor(255, 90, 90));
    palette.setColor(QPalette::Highlight, QColor(61, 139, 253));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(121, 184, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(103, 113, 132));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(103, 113, 132));
    app.setPalette(palette);

    QFile themeFile(QStringLiteral(":/theme/hxpainter_dark.qss"));
    if (!themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not load HXPainter theme stylesheet:" << themeFile.errorString();
        return;
    }

    QTextStream stream(&themeFile);
    app.setStyleSheet(stream.readAll());
}

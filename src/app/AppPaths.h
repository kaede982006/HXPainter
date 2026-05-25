#pragma once

#include <QString>
#include <QStringList>

class AppPaths {
public:
    static QString executableDir();
    static QString logoPath();
    static QStringList logoFallbackPaths();
    static QString qtPluginRoot();
    static QString platformsPluginDir();
    static QString logsDir();
    static QString diagnosticsLogPath();
};

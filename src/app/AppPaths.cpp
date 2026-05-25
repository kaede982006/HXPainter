#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>

QString AppPaths::executableDir()
{
    return QDir::cleanPath(QCoreApplication::applicationDirPath());
}

QString AppPaths::logoPath()
{
    return QDir(executableDir()).filePath("logo.png");
}

QStringList AppPaths::logoFallbackPaths()
{
    return {
        logoPath(),
        QStringLiteral(":/logo.png"),
        QStringLiteral(":/assets/logo.png"),
    };
}

QString AppPaths::qtPluginRoot()
{
    return executableDir();
}

QString AppPaths::platformsPluginDir()
{
    return QDir(qtPluginRoot()).filePath("platforms");
}

QString AppPaths::logsDir()
{
    return QDir(executableDir()).filePath("logs");
}

QString AppPaths::diagnosticsLogPath()
{
    QDir dir(executableDir());
    dir.mkpath("logs");
    return dir.filePath("logs/hxpainter.log");
}

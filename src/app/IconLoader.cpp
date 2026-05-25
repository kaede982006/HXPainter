#include "IconLoader.h"

#include "app/AppPaths.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QStyle>
#include <QTextStream>
#include <QDebug>

QIcon IconLoader::loadApplicationIcon()
{
    return loadApplicationIconWithDiagnostics().icon;
}

IconLoadResult IconLoader::loadApplicationIconWithDiagnostics()
{
    IconLoadResult result;
    const QString exeLogoPath = AppPaths::logoPath();

    for (const QString &path : AppPaths::logoFallbackPaths()) {
        const bool exists = path.startsWith(QStringLiteral(":/"))
            ? QFile::exists(path)
            : QFileInfo::exists(path);

        if (!exists) {
            if (path == exeLogoPath) {
                addMessage(result, QStringLiteral("logo.png not found beside executable: %1").arg(path));
            }
            continue;
        }

        QPixmap pixmap(path);
        if (pixmap.isNull()) {
            addMessage(result, QStringLiteral("logo candidate exists but could not be decoded: %1").arg(path));
            continue;
        }

        result.icon.addPixmap(pixmap);
        result.sourcePath = path;
        addMessage(result, QStringLiteral("Application icon loaded from: %1").arg(path));
        return result;
    }

    addMessage(result, QStringLiteral("Application logo not found. Expected: %1").arg(exeLogoPath));
    if (qApp != nullptr && qApp->style() != nullptr) {
        result.icon = qApp->style()->standardIcon(QStyle::SP_ComputerIcon);
        result.sourcePath = QStringLiteral("default-system-icon");
        addMessage(result, QStringLiteral("Using default system icon fallback."));
    }
    return result;
}

void IconLoader::appendDiagnostics(const QString &message)
{
    QFile file(AppPaths::diagnosticsLogPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open diagnostics log:" << file.fileName() << message;
        return;
    }

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate)
           << " " << message << Qt::endl;
}

void IconLoader::addMessage(IconLoadResult &result, const QString &message)
{
    result.messages.push_back(message);
    appendDiagnostics(message);
    qWarning().noquote() << message;
}

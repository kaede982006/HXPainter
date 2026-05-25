#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

struct IconLoadResult {
    QIcon icon;
    QString sourcePath;
    QStringList messages;
};

class IconLoader {
public:
    static QIcon loadApplicationIcon();
    static IconLoadResult loadApplicationIconWithDiagnostics();

private:
    static void appendDiagnostics(const QString &message);
    static void addMessage(IconLoadResult &result, const QString &message);
};

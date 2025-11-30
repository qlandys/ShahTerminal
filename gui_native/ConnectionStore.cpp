#include "ConnectionStore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString ensureConfigDir()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (path.isEmpty()) {
        path = QDir::homePath() + QLatin1String("/.shah_terminal");
    }
    QDir().mkpath(path);
    return path;
}
} // namespace

ConnectionStore::ConnectionStore(QObject *parent)
    : QObject(parent)
{
}

QString ConnectionStore::storagePath() const
{
    return ensureConfigDir();
}

QString ConnectionStore::credentialsFilePath() const
{
    return storagePath() + QLatin1String("/connections.json");
}

MexcCredentials ConnectionStore::loadMexcCredentials() const
{
    QFile file(credentialsFilePath());
    if (!file.exists()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (doc.isNull() || !doc.isObject()) {
        return {};
    }
    const QJsonObject root = doc.object();
    const QJsonObject mexcObj = root.value(QStringLiteral("mexcSpot")).toObject();
    MexcCredentials creds;
    creds.apiKey = mexcObj.value(QStringLiteral("apiKey")).toString();
    creds.saveSecret = mexcObj.value(QStringLiteral("saveSecret")).toBool(false);
    creds.viewOnly = mexcObj.value(QStringLiteral("viewOnly")).toBool(false);
    creds.autoConnect = mexcObj.value(QStringLiteral("autoConnect")).toBool(true);
    if (creds.saveSecret) {
        creds.secretKey = mexcObj.value(QStringLiteral("secretKey")).toString();
    }
    return creds;
}

void ConnectionStore::saveMexcCredentials(const MexcCredentials &creds)
{
    QFile file(credentialsFilePath());
    QJsonObject root;
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument existing = QJsonDocument::fromJson(file.readAll());
        if (existing.isObject()) {
            root = existing.object();
        }
        file.close();
    }

    QJsonObject mexcObj;
    mexcObj.insert(QStringLiteral("apiKey"), creds.apiKey);
    mexcObj.insert(QStringLiteral("saveSecret"), creds.saveSecret);
    mexcObj.insert(QStringLiteral("viewOnly"), creds.viewOnly);
    mexcObj.insert(QStringLiteral("autoConnect"), creds.autoConnect);
    if (creds.saveSecret) {
        mexcObj.insert(QStringLiteral("secretKey"), creds.secretKey);
    } else {
        mexcObj.remove(QStringLiteral("secretKey"));
    }
    root.insert(QStringLiteral("mexcSpot"), mexcObj);

    QSaveFile saveFile(credentialsFilePath());
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }
    QJsonDocument doc(root);
    saveFile.write(doc.toJson(QJsonDocument::Indented));
    saveFile.commit();
}

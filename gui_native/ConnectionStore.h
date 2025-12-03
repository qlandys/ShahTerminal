#pragma once

#include <QObject>
#include "TradeTypes.h"

class ConnectionStore : public QObject {
    Q_OBJECT

public:
    enum class Profile {
        MexcSpot,
        MexcFutures
    };

    explicit ConnectionStore(QObject *parent = nullptr);

    MexcCredentials loadMexcCredentials(Profile profile = Profile::MexcSpot) const;
    void saveMexcCredentials(const MexcCredentials &creds, Profile profile = Profile::MexcSpot);
    QString storagePath() const;

signals:
    void credentialsChanged(const QString &profileKey, const MexcCredentials &creds);

private:
    QString profileKey(Profile profile) const;
    QString credentialsFilePath() const;
};

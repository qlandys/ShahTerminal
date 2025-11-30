#pragma once

#include <QObject>
#include "TradeTypes.h"

class ConnectionStore : public QObject {
    Q_OBJECT

public:
    explicit ConnectionStore(QObject *parent = nullptr);

    MexcCredentials loadMexcCredentials() const;
    void saveMexcCredentials(const MexcCredentials &creds);
    QString storagePath() const;

private:
    QString credentialsFilePath() const;
};

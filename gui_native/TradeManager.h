#pragma once

#include "TradeTypes.h"
#include "ConnectionStore.h"

#include <QObject>
#include <QAbstractSocket>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QTimer>
#include <QWebSocket>

class TradeManager : public QObject {
    Q_OBJECT

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Error
    };
    Q_ENUM(ConnectionState)

    explicit TradeManager(QObject *parent = nullptr);

    void setCredentials(const MexcCredentials &creds);
    MexcCredentials credentials() const;
    ConnectionState state() const;
    void setProfile(ConnectionStore::Profile profile);
    ConnectionStore::Profile profile() const;

    void connectToExchange();
    void disconnect();
    bool isConnected() const;

    void placeLimitOrder(const QString &symbol, double price, double quantity, OrderSide side);
    void cancelAllOrders(const QString &symbol);

    TradePosition positionForSymbol(const QString &symbol) const;

signals:
    void connectionStateChanged(TradeManager::ConnectionState state, const QString &message);
    void orderPlaced(const QString &symbol, OrderSide side, double price, double quantity);
    void orderCanceled(const QString &symbol, OrderSide side, double price);
    void orderFailed(const QString &symbol, const QString &message);
    void positionChanged(const QString &symbol, const TradePosition &position);
    void logMessage(const QString &message);

private slots:
    void handleSocketConnected();
    void handleSocketDisconnected();
    void handleSocketError(QAbstractSocket::SocketError error);
    void handleSocketTextMessage(const QString &message);
    void handleSocketBinaryMessage(const QByteArray &payload);
    void refreshListenKey();

private:
    void setState(ConnectionState state, const QString &message = QString());
    QByteArray signPayload(const QUrlQuery &query) const;
    QByteArray signUzxPayload(const QByteArray &body,
                              const QString &method,
                              const QString &path) const;
    QNetworkRequest makePrivateRequest(const QString &path,
                                       const QUrlQuery &query,
                                       const QByteArray &contentType = QByteArray()) const;
    QNetworkRequest makeUzxRequest(const QString &path,
                                   const QByteArray &body,
                                   const QString &method = QStringLiteral("POST")) const;
    void handleOrderFill(const QString &symbol, OrderSide side, double price, double quantity);
    void emitPositionChanged(const QString &symbol);
    bool ensureCredentials() const;
    void requestListenKey();
    void initializeWebSocket(const QString &listenKey);
    void initializeUzxWebSocket();
    void closeWebSocket();
    void subscribePrivateChannels();
    void subscribeUzxPrivate();
    void sendListenKeyKeepAlive();
    void resetConnection(const QString &reason);
    void scheduleReconnect();
    void processPrivateDeal(const QByteArray &body, const QString &symbol);
    void processPrivateOrder(const QByteArray &body, const QString &symbol);
    void processPrivateAccount(const QByteArray &body);

    QNetworkAccessManager m_network;
    MexcCredentials m_credentials;
    ConnectionState m_state = ConnectionState::Disconnected;
    ConnectionStore::Profile m_profile = ConnectionStore::Profile::MexcSpot;
    QHash<QString, TradePosition> m_positions;
    const QString m_baseUrl = QStringLiteral("https://api.mexc.com");
    const QString m_uzxBaseUrl = QStringLiteral("https://api-v2.uzx.com");
    QWebSocket m_privateSocket;
    QTimer m_keepAliveTimer;
    QTimer m_reconnectTimer;
    QTimer m_wsPingTimer;
    QString m_listenKey;
    bool m_closingSocket = false;
    bool m_hasSubscribed = false;
};

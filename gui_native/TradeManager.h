#pragma once

#include "TradeTypes.h"

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

    void connectToExchange();
    void disconnect();
    bool isConnected() const;

    void placeLimitOrder(const QString &symbol, double price, double quantity, OrderSide side);

    TradePosition positionForSymbol(const QString &symbol) const;

signals:
    void connectionStateChanged(TradeManager::ConnectionState state, const QString &message);
    void orderPlaced(const QString &symbol, OrderSide side, double price, double quantity);
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
    QNetworkRequest makePrivateRequest(const QString &path,
                                       const QUrlQuery &query,
                                       const QByteArray &contentType = QByteArrayLiteral("application/x-www-form-urlencoded")) const;
    void handleOrderFill(const QString &symbol, OrderSide side, double price, double quantity);
    void emitPositionChanged(const QString &symbol);
    bool ensureCredentials() const;
    void requestListenKey();
    void initializeWebSocket(const QString &listenKey);
    void closeWebSocket();
    void subscribePrivateChannels();
    void sendListenKeyKeepAlive();
    void resetConnection(const QString &reason);
    void processPrivateDeal(const QByteArray &body, const QString &symbol);
    void processPrivateOrder(const QByteArray &body, const QString &symbol);
    void processPrivateAccount(const QByteArray &body);

    QNetworkAccessManager m_network;
    MexcCredentials m_credentials;
    ConnectionState m_state = ConnectionState::Disconnected;
    QHash<QString, TradePosition> m_positions;
    const QString m_baseUrl = QStringLiteral("https://api.mexc.com");
    QWebSocket m_privateSocket;
    QTimer m_keepAliveTimer;
    QString m_listenKey;
    bool m_closingSocket = false;
    bool m_hasSubscribed = false;
};

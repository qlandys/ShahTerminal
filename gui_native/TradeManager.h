#pragma once

#include "TradeTypes.h"
#include "ConnectionStore.h"
#include "DomWidget.h"

#include <QObject>
#include <QAbstractSocket>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
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
    ~TradeManager();

    void setCredentials(ConnectionStore::Profile profile, const MexcCredentials &creds);
    MexcCredentials credentials(ConnectionStore::Profile profile) const;
    ConnectionState state(ConnectionStore::Profile profile) const;
    ConnectionState overallState() const;

    void connectToExchange(ConnectionStore::Profile profile);
    void disconnect(ConnectionStore::Profile profile);
    bool isConnected(ConnectionStore::Profile profile) const;

    void placeLimitOrder(const QString &symbol,
                         const QString &accountName,
                         double price,
                         double quantity,
                         OrderSide side);
    void cancelAllOrders(const QString &symbol, const QString &accountName);

    TradePosition positionForSymbol(const QString &symbol, const QString &accountName) const;

signals:
    void connectionStateChanged(ConnectionStore::Profile profile,
                                TradeManager::ConnectionState state,
                                const QString &message);
    void orderPlaced(const QString &accountName,
                     const QString &symbol,
                     OrderSide side,
                     double price,
                     double quantity,
                     const QString &orderId);
    void orderCanceled(const QString &accountName,
                       const QString &symbol,
                       OrderSide side,
                       double price,
                       const QString &orderId);
    void orderFailed(const QString &accountName, const QString &symbol, const QString &message);
    void positionChanged(const QString &accountName,
                         const QString &symbol,
                         const TradePosition &position);
    void logMessage(const QString &message);
    void localOrdersUpdated(const QString &accountName,
                            const QString &symbol,
                            const QVector<DomWidget::LocalOrderMarker> &markers);

private:
    struct OrderRecord {
        QString symbol;
        OrderSide side = OrderSide::Buy;
        double price = 0.0;
        double quantityNotional = 0.0;
        qint64 createdMs = 0;
        QString orderId;
    };
    struct Context {
        ConnectionStore::Profile profile{ConnectionStore::Profile::MexcSpot};
        MexcCredentials credentials;
        QString accountName;
        ConnectionState state{ConnectionState::Disconnected};
        QWebSocket privateSocket;
        QTimer keepAliveTimer;
        QTimer reconnectTimer;
        QTimer wsPingTimer;
        QTimer openOrdersTimer;
        QString listenKey;
        bool closingSocket{false};
        bool hasSubscribed{false};
        bool openOrdersPending{false};
        QSet<QString> trackedSymbols;
        QHash<QString, TradePosition> positions;
        QHash<QString, OrderRecord> activeOrders;
        QSet<QString> pendingCancelSymbols;
    };

    Context &ensureContext(ConnectionStore::Profile profile) const;
    QString defaultAccountName(ConnectionStore::Profile profile) const;
    QString profileKey(ConnectionStore::Profile profile) const;
    QString accountNameFor(ConnectionStore::Profile profile) const;
    ConnectionStore::Profile profileFromAccountName(const QString &accountName) const;

    void setState(Context &ctx, ConnectionState state, const QString &message = QString());
    QByteArray signPayload(const QUrlQuery &query, const Context &ctx) const;
    QByteArray signUzxPayload(const QByteArray &body,
                              const QString &method,
                              const QString &path,
                              const Context &ctx) const;
    QNetworkRequest makePrivateRequest(const QString &path,
                                       const QUrlQuery &query,
                                       const QByteArray &contentType,
                                       const Context &ctx) const;
    QNetworkRequest makeUzxRequest(const QString &path,
                                   const QByteArray &body,
                                   const QString &method,
                                   const Context &ctx) const;
    void handleOrderFill(Context &ctx, const QString &symbol, OrderSide side, double price, double quantity);
    void emitPositionChanged(Context &ctx, const QString &symbol);
    bool ensureCredentials(const Context &ctx) const;
    void requestListenKey(Context &ctx);
    void initializeWebSocket(Context &ctx, const QString &listenKey);
    void initializeUzxWebSocket(Context &ctx);
    void closeWebSocket(Context &ctx);
    void subscribePrivateChannels(Context &ctx);
    void subscribeUzxPrivate(Context &ctx);
    void sendListenKeyKeepAlive(Context &ctx);
    void resetConnection(Context &ctx, const QString &reason);
    void scheduleReconnect(Context &ctx);
    void fetchOpenOrders(Context &ctx);
    void processPrivateDeal(Context &ctx, const QByteArray &body, const QString &symbol);
    void processPrivateOrder(Context &ctx, const QByteArray &body, const QString &symbol);
    void processPrivateAccount(Context &ctx, const QByteArray &body);
    void emitLocalOrderSnapshot(Context &ctx, const QString &symbol);
    void clearLocalOrderSnapshots(Context &ctx);
    void clearSymbolActiveOrders(Context &ctx, const QString &symbol);

    Context *contextForProfile(ConnectionStore::Profile profile) const;

    QNetworkAccessManager m_network;
    mutable QHash<ConnectionStore::Profile, Context *> m_contexts;
    const QString m_baseUrl = QStringLiteral("https://api.mexc.com");
    const QString m_uzxBaseUrl = QStringLiteral("https://api-v2.uzx.com");
};

#include "TradeManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QNetworkRequest>
#include <QUrl>
#include <QStringList>
#include <algorithm>
#include <cstddef>

namespace {
QString normalizedSymbol(const QString &symbol)
{
    QString s = symbol.trimmed().toUpper();
    return s;
}

struct ProtoReader
{
    ProtoReader(const void *ptr, std::size_t len)
        : data(static_cast<const quint8 *>(ptr))
        , size(len)
    {
    }

    bool eof() const { return pos >= size; }

    bool readVarint(quint64 &out)
    {
        out = 0;
        int shift = 0;
        while (pos < size && shift < 64) {
            const quint8 byte = data[pos++];
            out |= quint64(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) {
                return true;
            }
            shift += 7;
        }
        return false;
    }

    bool readLengthDelimited(QByteArray &out)
    {
        quint64 len = 0;
        if (!readVarint(len)) {
            return false;
        }
        if (pos + len > size) {
            return false;
        }
        out = QByteArray(reinterpret_cast<const char *>(data + pos), static_cast<int>(len));
        pos += static_cast<std::size_t>(len);
        return true;
    }

    bool skipField(quint64 key)
    {
        const auto wire = key & 0x7;
        switch (wire) {
        case 0: {
            quint64 dummy = 0;
            return readVarint(dummy);
        }
        case 1:
            if (pos + 8 > size) {
                return false;
            }
            pos += 8;
            return true;
        case 2: {
            quint64 len = 0;
            if (!readVarint(len) || pos + len > size) {
                return false;
            }
            pos += static_cast<std::size_t>(len);
            return true;
        }
        case 5:
            if (pos + 4 > size) {
                return false;
            }
            pos += 4;
            return true;
        default:
            return false;
        }
    }

    const quint8 *data = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
};

enum class PushBodyType { None, PrivateOrders, PrivateDeals, PrivateAccount };

struct PushMessage
{
    PushBodyType type = PushBodyType::None;
    QByteArray body;
    QString channel;
    QString symbol;
    qint64 sendTime = 0;
};

struct PrivateDealEvent
{
    double price = 0.0;
    double quantity = 0.0;
    int tradeType = 0;
    QString orderId;
    QString clientOrderId;
    qint64 time = 0;
    QString feeCurrency;
    double feeAmount = 0.0;
};

struct PrivateOrderEvent
{
    QString id;
    QString clientId;
    double price = 0.0;
    double quantity = 0.0;
    double avgPrice = 0.0;
    double remainQuantity = 0.0;
    double cumulativeQuantity = 0.0;
    double cumulativeAmount = 0.0;
    int status = 0;
    int tradeType = 0;
    qint64 createTime = 0;
};

struct PrivateAccountEvent
{
    QString asset;
    double balance = 0.0;
    double frozen = 0.0;
    QString changeType;
    qint64 time = 0;
};

double parseDecimal(const QByteArray &value)
{
    bool ok = false;
    const double v = QString::fromUtf8(value).toDouble(&ok);
    return ok ? v : 0.0;
}

QString parseString(const QByteArray &value)
{
    return QString::fromUtf8(value);
}

bool parsePushMessage(const QByteArray &payload, PushMessage &out)
{
    ProtoReader reader(payload.constData(), static_cast<std::size_t>(payload.size()));
    while (!reader.eof()) {
        quint64 key = 0;
        if (!reader.readVarint(key)) {
            return false;
        }
        const auto field = key >> 3;
        const auto wire = key & 0x7;
        if (wire == 2) {
            QByteArray value;
            if (!reader.readLengthDelimited(value)) {
                return false;
            }
            switch (field) {
            case 1:
                out.channel = parseString(value);
                break;
            case 3:
                out.symbol = parseString(value);
                break;
            case 304:
                out.type = PushBodyType::PrivateOrders;
                out.body = value;
                break;
            case 306:
                out.type = PushBodyType::PrivateDeals;
                out.body = value;
                break;
            case 307:
                out.type = PushBodyType::PrivateAccount;
                out.body = value;
                break;
            default:
                break;
            }
        } else if (wire == 0) {
            quint64 value = 0;
            if (!reader.readVarint(value)) {
                return false;
            }
            if (field == 6) {
                out.sendTime = static_cast<qint64>(value);
            }
        } else {
            if (!reader.skipField(key)) {
                return false;
            }
        }
    }
    return out.type != PushBodyType::None;
}

bool parsePrivateDealBody(const QByteArray &payload, PrivateDealEvent &event)
{
    ProtoReader reader(payload.constData(), static_cast<std::size_t>(payload.size()));
    while (!reader.eof()) {
        quint64 key = 0;
        if (!reader.readVarint(key)) {
            return false;
        }
        const auto field = key >> 3;
        const auto wire = key & 0x7;
        if (wire == 2) {
            QByteArray value;
            if (!reader.readLengthDelimited(value)) {
                return false;
            }
            switch (field) {
            case 1:
                event.price = parseDecimal(value);
                break;
            case 2:
                event.quantity = parseDecimal(value);
                break;
            case 7:
                event.orderId = parseString(value);
                break;
            case 8:
                event.clientOrderId = parseString(value);
                break;
            case 10:
                event.feeAmount = parseDecimal(value);
                break;
            case 11:
                event.feeCurrency = parseString(value);
                break;
            default:
                break;
            }
        } else if (wire == 0) {
            quint64 value = 0;
            if (!reader.readVarint(value)) {
                return false;
            }
            switch (field) {
            case 4:
                event.tradeType = static_cast<int>(value);
                break;
            case 12:
                event.time = static_cast<qint64>(value);
                break;
            default:
                break;
            }
        } else {
            if (!reader.skipField(key)) {
                return false;
            }
        }
    }
    return true;
}

bool parsePrivateOrderBody(const QByteArray &payload, PrivateOrderEvent &event)
{
    ProtoReader reader(payload.constData(), static_cast<std::size_t>(payload.size()));
    while (!reader.eof()) {
        quint64 key = 0;
        if (!reader.readVarint(key)) {
            return false;
        }
        const auto field = key >> 3;
        const auto wire = key & 0x7;
        if (wire == 2) {
            QByteArray value;
            if (!reader.readLengthDelimited(value)) {
                return false;
            }
            switch (field) {
            case 1:
                event.id = parseString(value);
                break;
            case 2:
                event.clientId = parseString(value);
                break;
            case 3:
                event.price = parseDecimal(value);
                break;
            case 4:
                event.quantity = parseDecimal(value);
                break;
            case 6:
                event.avgPrice = parseDecimal(value);
                break;
            case 11:
                event.remainQuantity = parseDecimal(value);
                break;
            case 13:
                event.cumulativeQuantity = parseDecimal(value);
                break;
            case 14:
                event.cumulativeAmount = parseDecimal(value);
                break;
            default:
                break;
            }
        } else if (wire == 0) {
            quint64 value = 0;
            if (!reader.readVarint(value)) {
                return false;
            }
            switch (field) {
            case 8:
                event.tradeType = static_cast<int>(value);
                break;
            case 15:
                event.status = static_cast<int>(value);
                break;
            case 16:
                event.createTime = static_cast<qint64>(value);
                break;
            default:
                break;
            }
        } else {
            if (!reader.skipField(key)) {
                return false;
            }
        }
    }
    return true;
}

bool parsePrivateAccountBody(const QByteArray &payload, PrivateAccountEvent &event)
{
    ProtoReader reader(payload.constData(), static_cast<std::size_t>(payload.size()));
    while (!reader.eof()) {
        quint64 key = 0;
        if (!reader.readVarint(key)) {
            return false;
        }
        const auto field = key >> 3;
        const auto wire = key & 0x7;
        if (wire == 2) {
            QByteArray value;
            if (!reader.readLengthDelimited(value)) {
                return false;
            }
            switch (field) {
            case 1:
                event.asset = parseString(value);
                break;
            case 3:
                event.balance = parseDecimal(value);
                break;
            case 5:
                event.frozen = parseDecimal(value);
                break;
            case 7:
                event.changeType = parseString(value);
                break;
            default:
                break;
            }
        } else if (wire == 0) {
            quint64 value = 0;
            if (!reader.readVarint(value)) {
                return false;
            }
            if (field == 8) {
                event.time = static_cast<qint64>(value);
            }
        } else {
            if (!reader.skipField(key)) {
                return false;
            }
        }
    }
    return true;
}

QString statusText(int status)
{
    switch (status) {
    case 1:
        return QStringLiteral("NEW");
    case 2:
        return QStringLiteral("FILLED");
    case 3:
        return QStringLiteral("PARTIALLY_FILLED");
    case 4:
        return QStringLiteral("CANCELED");
    case 5:
        return QStringLiteral("PARTIALLY_CANCELED");
    default:
        return QString::number(status);
    }
}
} // namespace

TradeManager::TradeManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<TradePosition>("TradePosition");
    qRegisterMetaType<MexcCredentials>("MexcCredentials");

    connect(&m_privateSocket, &QWebSocket::connected, this, &TradeManager::handleSocketConnected);
    connect(&m_privateSocket, &QWebSocket::disconnected, this, &TradeManager::handleSocketDisconnected);
    connect(&m_privateSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this,
            &TradeManager::handleSocketError);
    connect(&m_privateSocket,
            &QWebSocket::textMessageReceived,
            this,
            &TradeManager::handleSocketTextMessage);
    connect(&m_privateSocket,
            &QWebSocket::binaryMessageReceived,
            this,
            &TradeManager::handleSocketBinaryMessage);

    m_keepAliveTimer.setInterval(25 * 60 * 1000);
    connect(&m_keepAliveTimer, &QTimer::timeout, this, &TradeManager::refreshListenKey);
}

void TradeManager::setCredentials(const MexcCredentials &creds)
{
    m_credentials = creds;
}

MexcCredentials TradeManager::credentials() const
{
    return m_credentials;
}

TradeManager::ConnectionState TradeManager::state() const
{
    return m_state;
}

bool TradeManager::ensureCredentials() const
{
    return !m_credentials.apiKey.isEmpty() && !m_credentials.secretKey.isEmpty();
}

void TradeManager::connectToExchange()
{
    if (!ensureCredentials()) {
        setState(ConnectionState::Error, QStringLiteral("Missing API key or secret"));
        emit logMessage(QStringLiteral("Provide both API key and secret to connect."));
        return;
    }
    if (m_state == ConnectionState::Connecting) {
        return;
    }
    closeWebSocket();
    m_listenKey.clear();
    m_hasSubscribed = false;
    setState(ConnectionState::Connecting, QStringLiteral("Requesting listen key..."));
    emit logMessage(QStringLiteral("Requesting listen key from MEXC..."));
    requestListenKey();
}

void TradeManager::disconnect()
{
    if (m_state == ConnectionState::Disconnected) {
        return;
    }
    closeWebSocket();
    m_listenKey.clear();
    m_hasSubscribed = false;
    setState(ConnectionState::Disconnected, QStringLiteral("Disconnected"));
    emit logMessage(QStringLiteral("Disconnected from MEXC"));
}

bool TradeManager::isConnected() const
{
    return m_state == ConnectionState::Connected;
}

void TradeManager::placeLimitOrder(const QString &symbol,
                                   double price,
                                   double quantity,
                                   OrderSide side)
{
    const QString sym = normalizedSymbol(symbol);
    if (!ensureCredentials()) {
        emit orderFailed(sym, QStringLiteral("Missing credentials"));
        return;
    }
    if (!isConnected()) {
        emit orderFailed(sym, QStringLiteral("Connect to the exchange first"));
        return;
    }
    if (price <= 0.0 || quantity <= 0.0) {
        emit orderFailed(sym, QStringLiteral("Invalid price or quantity"));
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("symbol"), sym);
    query.addQueryItem(QStringLiteral("side"), side == OrderSide::Buy ? QStringLiteral("BUY") : QStringLiteral("SELL"));
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("LIMIT"));
    query.addQueryItem(QStringLiteral("timeInForce"), QStringLiteral("GTC"));
    query.addQueryItem(QStringLiteral("price"), QString::number(price, 'f', 8));
    query.addQueryItem(QStringLiteral("quantity"), QString::number(quantity, 'f', 8));
    query.addQueryItem(QStringLiteral("recvWindow"), QStringLiteral("5000"));
    query.addQueryItem(QStringLiteral("timestamp"), QString::number(QDateTime::currentMSecsSinceEpoch()));

    QUrlQuery signedQuery = query;
    signedQuery.addQueryItem(QStringLiteral("signature"), QString::fromLatin1(signPayload(query)));
    const QByteArray payload = signedQuery.query(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest request = makePrivateRequest(QStringLiteral("/api/v3/order"), QUrlQuery());
    auto *reply = m_network.post(request, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply, sym, side, price, quantity]() {
        const QNetworkReply::NetworkError err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            emit orderFailed(sym, reply->errorString());
            emit logMessage(QStringLiteral("Order error: %1").arg(reply->errorString()));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isNull() || !doc.isObject()) {
            emit orderFailed(sym, QStringLiteral("Invalid response"));
            return;
        }
        const QJsonObject obj = doc.object();
        if (obj.contains(QStringLiteral("code")) && obj.value(QStringLiteral("code")).toInt(0) != 0) {
            const QString msg = obj.value(QStringLiteral("msg")).toString(QStringLiteral("Unknown error"));
            emit orderFailed(sym, msg);
            emit logMessage(QStringLiteral("Order rejected: %1").arg(msg));
            return;
        }
        emit orderPlaced(sym, side, price, quantity);
        emit logMessage(QStringLiteral("Order accepted: %1 %2 @ %3")
                            .arg(side == OrderSide::Buy ? QStringLiteral("BUY") : QStringLiteral("SELL"))
                            .arg(quantity, 0, 'f', 4)
                            .arg(price, 0, 'f', 5));
    });
}

TradePosition TradeManager::positionForSymbol(const QString &symbol) const
{
    return m_positions.value(normalizedSymbol(symbol), TradePosition{});
}

void TradeManager::setState(ConnectionState state, const QString &message)
{
    if (m_state == state && message.isEmpty()) {
        return;
    }
    m_state = state;
    emit connectionStateChanged(state, message);
}

QByteArray TradeManager::signPayload(const QUrlQuery &query) const
{
    const QByteArray payload = query.query(QUrl::FullyEncoded).toUtf8();
    return QMessageAuthenticationCode::hash(payload, m_credentials.secretKey.toUtf8(), QCryptographicHash::Sha256).toHex();
}

QNetworkRequest TradeManager::makePrivateRequest(const QString &path,
                                                 const QUrlQuery &query,
                                                 const QByteArray &contentType) const
{
    QUrl url(m_baseUrl + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkRequest req(url);
    if (!contentType.isEmpty()) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }
    req.setRawHeader("X-MEXC-APIKEY", m_credentials.apiKey.toUtf8());
    return req;
}

void TradeManager::handleOrderFill(const QString &symbol, OrderSide side, double price, double quantity)
{
    const QString sym = normalizedSymbol(symbol);
    TradePosition &pos = m_positions[sym];
    if (!pos.hasPosition) {
        pos.hasPosition = true;
        pos.side = side;
        pos.averagePrice = price;
        pos.quantity = quantity;
    } else if (pos.side == side) {
        const double totalNotional = pos.averagePrice * pos.quantity + price * quantity;
        pos.quantity += quantity;
        if (pos.quantity > 1e-9) {
            pos.averagePrice = totalNotional / pos.quantity;
        } else {
            pos.averagePrice = price;
        }
    } else {
        const double closingQty = std::min(pos.quantity, quantity);
        double pnl = 0.0;
        if (pos.side == OrderSide::Buy) {
            pnl = (price - pos.averagePrice) * closingQty;
        } else {
            pnl = (pos.averagePrice - price) * closingQty;
        }
        pos.realizedPnl += pnl;
        pos.quantity -= closingQty;
        if (pos.quantity <= 1e-8) {
            pos.hasPosition = false;
            pos.quantity = 0.0;
            pos.averagePrice = 0.0;
            pos.side = side;
        }
        const double remainder = quantity - closingQty;
        if (remainder > 1e-8) {
            TradePosition &newPos = m_positions[sym];
            if (!newPos.hasPosition) {
                newPos.hasPosition = true;
                newPos.side = side;
                newPos.quantity = remainder;
                newPos.averagePrice = price;
            } else if (newPos.side == side) {
                const double total = newPos.averagePrice * newPos.quantity + price * remainder;
                newPos.quantity += remainder;
                newPos.averagePrice = total / newPos.quantity;
            }
        }
    }
    emitPositionChanged(sym);
}

void TradeManager::emitPositionChanged(const QString &symbol)
{
    emit positionChanged(symbol, m_positions.value(symbol));
}

void TradeManager::requestListenKey()
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("timestamp"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("recvWindow"), QStringLiteral("5000"));
    QUrlQuery signedQuery = query;
    signedQuery.addQueryItem(QStringLiteral("signature"), QString::fromLatin1(signPayload(query)));
    QNetworkRequest request = makePrivateRequest(QStringLiteral("/api/v3/userDataStream"),
                                                 signedQuery,
                                                 QByteArrayLiteral("application/json"));
    auto *reply = m_network.post(request, QByteArrayLiteral("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QNetworkReply::NetworkError err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            const QString message = raw.isEmpty() ? reply->errorString() : QString::fromUtf8(raw);
            resetConnection(message);
            emit logMessage(QStringLiteral("Listen key request failed: %1").arg(message));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        const QJsonObject obj = doc.object();
        const QString listenKey = obj.value(QStringLiteral("listenKey")).toString();
        if (listenKey.isEmpty()) {
            resetConnection(QStringLiteral("Listen key missing in response"));
            emit logMessage(QStringLiteral("Unexpected listen key payload: %1").arg(QString::fromUtf8(raw)));
            return;
        }
        emit logMessage(QStringLiteral("Received listen key, opening private WebSocket..."));
        initializeWebSocket(listenKey);
    });
}

void TradeManager::initializeWebSocket(const QString &listenKey)
{
    m_listenKey = listenKey;
    if (m_privateSocket.state() != QAbstractSocket::UnconnectedState) {
        m_closingSocket = true;
        m_privateSocket.close();
    }
    QUrl url(QStringLiteral("wss://wbs-api.mexc.com/ws"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("listenKey"), listenKey);
    url.setQuery(query);
    emit logMessage(QStringLiteral("Connecting to %1").arg(url.toString(QUrl::RemoveUserInfo)));
    m_privateSocket.open(url);
}

void TradeManager::closeWebSocket()
{
    m_keepAliveTimer.stop();
    if (m_privateSocket.state() != QAbstractSocket::UnconnectedState) {
        m_closingSocket = true;
        m_privateSocket.close();
    }
}

void TradeManager::subscribePrivateChannels()
{
    if (m_privateSocket.state() != QAbstractSocket::ConnectedState || m_hasSubscribed) {
        return;
    }
    const QStringList channels{QStringLiteral("spot@private.orders.v3.api.pb"),
                               QStringLiteral("spot@private.deals.v3.api.pb"),
                               QStringLiteral("spot@private.account.v3.api.pb")};
    QJsonObject payload;
    payload.insert(QStringLiteral("method"), QStringLiteral("SUBSCRIPTION"));
    payload.insert(QStringLiteral("params"), QJsonArray::fromStringList(channels));
    payload.insert(QStringLiteral("id"), 1);
    const QString message = QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_privateSocket.sendTextMessage(message);
    emit logMessage(QStringLiteral("Subscribed to private channels."));
    m_hasSubscribed = true;
}

void TradeManager::sendListenKeyKeepAlive()
{
    if (m_listenKey.isEmpty()) {
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("listenKey"), m_listenKey);
    query.addQueryItem(QStringLiteral("timestamp"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("recvWindow"), QStringLiteral("5000"));
    QUrlQuery signedQuery = query;
    signedQuery.addQueryItem(QStringLiteral("signature"), QString::fromLatin1(signPayload(query)));
    QJsonObject payloadObj{{QStringLiteral("listenKey"), m_listenKey}};
    const QByteArray payload = QJsonDocument(payloadObj).toJson(QJsonDocument::Compact);
    QNetworkRequest request = makePrivateRequest(QStringLiteral("/api/v3/userDataStream"),
                                                 signedQuery,
                                                 QByteArrayLiteral("application/json"));
    auto *reply = m_network.put(request, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QNetworkReply::NetworkError err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            const QString message = raw.isEmpty() ? reply->errorString() : QString::fromUtf8(raw);
            emit logMessage(QStringLiteral("Keepalive failed: %1").arg(message));
        } else {
            emit logMessage(QStringLiteral("Listen key refreshed."));
        }
    });
}

void TradeManager::resetConnection(const QString &reason)
{
    closeWebSocket();
    m_hasSubscribed = false;
    m_listenKey.clear();
    setState(ConnectionState::Error, reason);
}

void TradeManager::handleSocketConnected()
{
    emit logMessage(QStringLiteral("Private WebSocket connected."));
    subscribePrivateChannels();
    sendListenKeyKeepAlive();
    if (!m_keepAliveTimer.isActive()) {
        m_keepAliveTimer.start();
    }
    setState(ConnectionState::Connected, QStringLiteral("Connected to private WebSocket"));
}

void TradeManager::handleSocketDisconnected()
{
    m_keepAliveTimer.stop();
    if (m_closingSocket) {
        m_closingSocket = false;
        emit logMessage(QStringLiteral("Private WebSocket closed."));
        return;
    }
    emit logMessage(QStringLiteral("Private WebSocket disconnected unexpectedly."));
    setState(ConnectionState::Error, QStringLiteral("WebSocket disconnected"));
}

void TradeManager::handleSocketError(QAbstractSocket::SocketError)
{
    if (m_closingSocket) {
        return;
    }
    emit logMessage(QStringLiteral("WebSocket error: %1").arg(m_privateSocket.errorString()));
    if (m_state != ConnectionState::Error) {
        setState(ConnectionState::Error, m_privateSocket.errorString());
    }
}

void TradeManager::handleSocketTextMessage(const QString &message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        emit logMessage(QStringLiteral("WS text: %1").arg(message));
        return;
    }
    const QJsonObject obj = doc.object();
    const QString method = obj.value(QStringLiteral("method")).toString().toUpper();
    if (method == QStringLiteral("PING")) {
        m_privateSocket.sendTextMessage(QStringLiteral("{\"method\":\"PONG\"}"));
        return;
    }
    if (obj.contains(QStringLiteral("code"))) {
        emit logMessage(QStringLiteral("WS event: %1").arg(message));
    }
}

void TradeManager::handleSocketBinaryMessage(const QByteArray &payload)
{
    PushMessage message;
    if (!parsePushMessage(payload, message)) {
        emit logMessage(QStringLiteral("Failed to decode private WS payload."));
        return;
    }

    switch (message.type) {
    case PushBodyType::PrivateDeals:
        processPrivateDeal(message.body, message.symbol);
        break;
    case PushBodyType::PrivateOrders:
        processPrivateOrder(message.body, message.symbol);
        break;
    case PushBodyType::PrivateAccount:
        processPrivateAccount(message.body);
        break;
    default:
        break;
    }
}

void TradeManager::refreshListenKey()
{
    sendListenKeyKeepAlive();
}

void TradeManager::processPrivateDeal(const QByteArray &body, const QString &symbol)
{
    if (symbol.isEmpty()) {
        emit logMessage(QStringLiteral("Private deal payload missing symbol."));
        return;
    }
    PrivateDealEvent event;
    if (!parsePrivateDealBody(body, event)) {
        emit logMessage(QStringLiteral("Failed to parse private deal payload."));
        return;
    }
    if (event.quantity <= 0.0 || event.price <= 0.0) {
        return;
    }

    const QString sym = normalizedSymbol(symbol);
    const OrderSide side = event.tradeType == 1 ? OrderSide::Buy : OrderSide::Sell;
    handleOrderFill(sym, side, event.price, event.quantity);
    emit logMessage(QStringLiteral("Deal %1 %2 %3 @ %4 (order %5)")
                        .arg(sym)
                        .arg(side == OrderSide::Buy ? QStringLiteral("BUY") : QStringLiteral("SELL"))
                        .arg(event.quantity, 0, 'f', 8)
                        .arg(event.price, 0, 'f', 8)
                        .arg(event.orderId));
}

void TradeManager::processPrivateOrder(const QByteArray &body, const QString &symbol)
{
    PrivateOrderEvent event;
    if (!parsePrivateOrderBody(body, event)) {
        emit logMessage(QStringLiteral("Failed to parse private order payload."));
        return;
    }

    const QString identifier = !event.id.isEmpty() ? event.id : event.clientId;
    emit logMessage(QStringLiteral("Order %1 (%2): status=%3 remain=%4 cumQty=%5 @avg %6")
                        .arg(identifier)
                        .arg(symbol)
                        .arg(statusText(event.status))
                        .arg(event.remainQuantity, 0, 'f', 8)
                        .arg(event.cumulativeQuantity, 0, 'f', 8)
                        .arg(event.avgPrice, 0, 'f', 8));
}

void TradeManager::processPrivateAccount(const QByteArray &body)
{
    PrivateAccountEvent event;
    if (!parsePrivateAccountBody(body, event)) {
        emit logMessage(QStringLiteral("Failed to parse private account payload."));
        return;
    }
    emit logMessage(QStringLiteral("Balance %1: available=%2 frozen=%3 (%4)")
                        .arg(event.asset)
                        .arg(event.balance, 0, 'f', 8)
                        .arg(event.frozen, 0, 'f', 8)
                        .arg(event.changeType));
}

#include "TradeManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMessageAuthenticationCode>
#include <QNetworkRequest>
#include <QUrl>
#include <QStringList>
#include <QSet>
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
    return true;
}

bool parsePrivateDealBody(const QByteArray &payload, PrivateDealEvent &out)
{
    ProtoReader reader(payload.constData(), static_cast<std::size_t>(payload.size()));
    while (!reader.eof()) {
        quint64 key = 0;
        if (!reader.readVarint(key)) {
            return false;
        }
        const auto field = key >> 3;
        if ((key & 0x7) == 2) {
            QByteArray value;
            if (!reader.readLengthDelimited(value)) {
                return false;
            }
            switch (field) {
            case 1:
                out.orderId = parseString(value);
                break;
            case 2:
                out.clientOrderId = parseString(value);
                break;
            case 9:
                out.price = parseDecimal(value);
                break;
            case 8:
                out.quantity = parseDecimal(value);
                break;
            case 10:
                out.tradeType = parseString(value).toInt();
                break;
            case 11:
                out.time = parseString(value).toLongLong();
                break;
            case 12:
                out.feeCurrency = parseString(value);
                break;
            case 13:
                out.feeAmount = parseDecimal(value);
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

bool parsePrivateOrderBody(const QByteArray &payload, PrivateOrderEvent &out)
{
    ProtoReader reader(payload.constData(), static_cast<std::size_t>(payload.size()));
    while (!reader.eof()) {
        quint64 key = 0;
        if (!reader.readVarint(key)) {
            return false;
        }
        const auto field = key >> 3;
        if ((key & 0x7) == 2) {
            QByteArray value;
            if (!reader.readLengthDelimited(value)) {
                return false;
            }
            switch (field) {
            case 1:
                out.id = parseString(value);
                break;
            case 2:
                out.clientId = parseString(value);
                break;
            case 3:
                out.price = parseDecimal(value);
                break;
            case 4:
                out.quantity = parseDecimal(value);
                break;
            case 5:
                out.avgPrice = parseDecimal(value);
                break;
            case 6:
                out.remainQuantity = parseDecimal(value);
                break;
            case 7:
                out.cumulativeQuantity = parseDecimal(value);
                break;
            case 8:
                out.cumulativeAmount = parseDecimal(value);
                break;
            case 9:
                out.status = parseString(value).toInt();
                break;
            case 10:
                out.tradeType = parseString(value).toInt();
                break;
            case 13:
                out.createTime = parseString(value).toLongLong();
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

bool parsePrivateAccountBody(const QByteArray &payload, PrivateAccountEvent &out)
{
    ProtoReader reader(payload.constData(), static_cast<std::size_t>(payload.size()));
    while (!reader.eof()) {
        quint64 key = 0;
        if (!reader.readVarint(key)) {
            return false;
        }
        const auto field = key >> 3;
        if ((key & 0x7) == 2) {
            QByteArray value;
            if (!reader.readLengthDelimited(value)) {
                return false;
            }
            switch (field) {
            case 1:
                out.asset = parseString(value);
                break;
            case 2:
                out.balance = parseDecimal(value);
                break;
            case 3:
                out.frozen = parseDecimal(value);
                break;
            case 4:
                out.changeType = parseString(value);
                break;
            case 6:
                out.time = parseString(value).toLongLong();
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

QString statusText(int status)
{
    switch (status) {
    case 2:
        return QStringLiteral("FILLED");
    case 4:
        return QStringLiteral("CANCELED");
    case 5:
        return QStringLiteral("PARTIALLY_CANCELED");
    default:
        return QString::number(status);
    }
}

QString contextTag(const QString &accountName)
{
    QString label = accountName;
    if (label.isEmpty()) {
        label = QStringLiteral("account");
    }
    return QStringLiteral("[%1]").arg(label);
}

QString uzxWireSymbol(const QString &userSymbol, bool isSwap)
{
    QString sym = userSymbol.trimmed().toUpper();
    if (sym.isEmpty()) return sym;
    if (isSwap) {
        return sym.replace(QStringLiteral("-"), QString());
    }
    if (!sym.contains(QLatin1Char('-'))) {
        static const QStringList quotes = {
            QStringLiteral("USDT"), QStringLiteral("USDC"), QStringLiteral("USDR"),
            QStringLiteral("USDQ"), QStringLiteral("EURQ"), QStringLiteral("EURR"),
            QStringLiteral("BTC"), QStringLiteral("ETH")};
        for (const QString &q : quotes) {
            if (sym.endsWith(q, Qt::CaseInsensitive)) {
                const QString base = sym.left(sym.size() - q.size());
                if (!base.isEmpty()) {
                    sym = base + QLatin1Char('-') + q;
                }
                break;
            }
        }
    }
    return sym;
}

} // namespace
TradeManager::TradeManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<TradePosition>("TradePosition");
    qRegisterMetaType<MexcCredentials>("MexcCredentials");
}

TradeManager::~TradeManager()
{
    qDeleteAll(m_contexts);
}

void TradeManager::setCredentials(ConnectionStore::Profile profile, const MexcCredentials &creds)
{
    Context &ctx = ensureContext(profile);
    ctx.credentials = creds;
    QString account = creds.label.trimmed();
    if (account.isEmpty()) {
        account = defaultAccountName(profile);
    }
    ctx.accountName = account;
}

MexcCredentials TradeManager::credentials(ConnectionStore::Profile profile) const
{
    if (Context *ctx = contextForProfile(profile)) {
        return ctx->credentials;
    }
    return MexcCredentials{};
}

TradeManager::ConnectionState TradeManager::state(ConnectionStore::Profile profile) const
{
    if (Context *ctx = contextForProfile(profile)) {
        return ctx->state;
    }
    return ConnectionState::Disconnected;
}

TradeManager::ConnectionState TradeManager::overallState() const
{
    bool hasConnected = false;
    bool hasConnecting = false;
    for (auto it = m_contexts.constBegin(); it != m_contexts.constEnd(); ++it) {
        switch (it.value()->state) {
        case ConnectionState::Error:
            return ConnectionState::Error;
        case ConnectionState::Connecting:
            hasConnecting = true;
            break;
        case ConnectionState::Connected:
            hasConnected = true;
            break;
        case ConnectionState::Disconnected:
        default:
            break;
        }
    }
    if (hasConnecting) {
        return ConnectionState::Connecting;
    }
    if (hasConnected) {
        return ConnectionState::Connected;
    }
    return ConnectionState::Disconnected;
}

bool TradeManager::ensureCredentials(const Context &ctx) const
{
    if (ctx.profile == ConnectionStore::Profile::UzxSpot
        || ctx.profile == ConnectionStore::Profile::UzxSwap) {
        return !ctx.credentials.apiKey.isEmpty() && !ctx.credentials.secretKey.isEmpty()
               && !ctx.credentials.passphrase.isEmpty();
    }
    return !ctx.credentials.apiKey.isEmpty() && !ctx.credentials.secretKey.isEmpty();
}

void TradeManager::connectToExchange(ConnectionStore::Profile profile)
{
    Context &ctx = ensureContext(profile);
    if (!ensureCredentials(ctx)) {
        setState(ctx, ConnectionState::Error, tr("Missing API credentials"));
        emit logMessage(QStringLiteral("%1 Provide API key/secret (and passphrase for UZX).")
                            .arg(contextTag(ctx.accountName)));
        return;
    }
    if (ctx.state == ConnectionState::Connecting) {
        return;
    }
    closeWebSocket(ctx);
    ctx.listenKey.clear();
    ctx.hasSubscribed = false;
    if (ctx.profile == ConnectionStore::Profile::UzxSpot
        || ctx.profile == ConnectionStore::Profile::UzxSwap) {
        setState(ctx, ConnectionState::Connecting, tr("Connecting to UZX..."));
        emit logMessage(QStringLiteral("%1 Connecting to UZX private WebSocket...")
                            .arg(contextTag(ctx.accountName)));
        initializeUzxWebSocket(ctx);
    } else {
        setState(ctx, ConnectionState::Connecting, tr("Requesting listen key..."));
        emit logMessage(QStringLiteral("%1 Requesting listen key from MEXC...")
                            .arg(contextTag(ctx.accountName)));
        requestListenKey(ctx);
    }
}

void TradeManager::disconnect(ConnectionStore::Profile profile)
{
    Context *ctx = contextForProfile(profile);
    if (!ctx) {
        return;
    }
    closeWebSocket(*ctx);
    clearLocalOrderSnapshots(*ctx);
    ctx->listenKey.clear();
    ctx->hasSubscribed = false;
    setState(*ctx, ConnectionState::Disconnected, tr("Disconnected"));
    emit logMessage(QStringLiteral("%1 Disconnected").arg(contextTag(ctx->accountName)));
}

bool TradeManager::isConnected(ConnectionStore::Profile profile) const
{
    return state(profile) == ConnectionState::Connected;
}

QString TradeManager::accountNameFor(ConnectionStore::Profile profile) const
{
    if (Context *ctx = contextForProfile(profile)) {
        return ctx->accountName;
    }
    return defaultAccountName(profile);
}

ConnectionStore::Profile TradeManager::profileFromAccountName(const QString &accountName) const
{
    if (accountName.isEmpty()) {
        return ConnectionStore::Profile::MexcSpot;
    }
    const QString lower = accountName.trimmed().toLower();
    for (auto it = m_contexts.constBegin(); it != m_contexts.constEnd(); ++it) {
        if (it.value()->accountName.trimmed().toLower() == lower) {
            return it.key();
        }
    }
    if (lower.contains(QStringLiteral("futures"))) {
        return ConnectionStore::Profile::MexcFutures;
    }
    if (lower.contains(QStringLiteral("swap"))) {
        return ConnectionStore::Profile::UzxSwap;
    }
    if (lower.contains(QStringLiteral("spot")) && lower.contains(QStringLiteral("uzx"))) {
        return ConnectionStore::Profile::UzxSpot;
    }
    return ConnectionStore::Profile::MexcSpot;
}

TradePosition TradeManager::positionForSymbol(const QString &symbol, const QString &accountName) const
{
    const ConnectionStore::Profile profile = profileFromAccountName(accountName);
    if (Context *ctx = contextForProfile(profile)) {
        return ctx->positions.value(normalizedSymbol(symbol), TradePosition{});
    }
    return TradePosition{};
}
void TradeManager::placeLimitOrder(const QString &symbol,
                                   const QString &accountName,
                                   double price,
                                   double quantity,
                                   OrderSide side)
{
    const QString sym = normalizedSymbol(symbol);
    const ConnectionStore::Profile profile = profileFromAccountName(accountName);
    Context &ctx = ensureContext(profile);
    if (!ensureCredentials(ctx)) {
        emit orderFailed(ctx.accountName, sym, tr("Missing credentials"));
        return;
    }
    if (ctx.state != ConnectionState::Connected) {
        emit orderFailed(ctx.accountName, sym, tr("Connect to the exchange first"));
        return;
    }
    if (price <= 0.0 || quantity <= 0.0) {
        emit orderFailed(ctx.accountName, sym, tr("Invalid price or quantity"));
        return;
    }

    emit logMessage(QStringLiteral("%1 Placing limit order: %2 %3 @ %4 qty=%5")
                        .arg(contextTag(ctx.accountName))
                        .arg(sym)
                        .arg(side == OrderSide::Buy ? QStringLiteral("BUY") : QStringLiteral("SELL"))
                        .arg(QString::number(price, 'f', 6))
                        .arg(QString::number(quantity, 'f', 6)));

    if (profile == ConnectionStore::Profile::UzxSwap
        || profile == ConnectionStore::Profile::UzxSpot) {
        const bool isSwap = (profile == ConnectionStore::Profile::UzxSwap);
        const QString wireSym = uzxWireSymbol(sym, isSwap);
        QJsonObject payload;
        const QString priceStr = QString::number(price, 'f', 8);
        const QString amountStr = QString::number(quantity, 'f', 8);
        payload.insert(QStringLiteral("product_name"), wireSym);
        payload.insert(QStringLiteral("order_type"), 2);
        payload.insert(QStringLiteral("price"), priceStr);
        payload.insert(QStringLiteral("amount"), amountStr);
        payload.insert(QStringLiteral("order_buy_or_sell"), side == OrderSide::Buy ? 1 : 2);
        if (isSwap) {
            payload.insert(QStringLiteral("number"), amountStr);
            payload.insert(QStringLiteral("trade_ccy"), 1);
            payload.insert(QStringLiteral("pos_side"),
                           side == OrderSide::Buy ? QStringLiteral("LG") : QStringLiteral("ST"));
        }
        const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        emit logMessage(QStringLiteral("%1 UZX REST body: %2")
                            .arg(contextTag(ctx.accountName), QString::fromUtf8(body)));
        const QString path = isSwap ? QStringLiteral("/v2/trade/swap/order")
                                    : QStringLiteral("/v2/trade/spot/order");
        QNetworkRequest req = makeUzxRequest(path, body, QStringLiteral("POST"), ctx);
        auto *reply = m_network.post(req, body);
        connect(reply,
                &QNetworkReply::finished,
                this,
                [this, reply, sym, side, price, quantity, ctxPtr = &ctx]() {
                    const auto err = reply->error();
                    const int status =
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    const QByteArray raw = reply->readAll();
                    reply->deleteLater();
                    if (err != QNetworkReply::NoError || status >= 400) {
                        QString msg = reply->errorString();
                        if (status >= 400) {
                            msg = QStringLiteral("HTTP %1: %2")
                                      .arg(status)
                                      .arg(QString::fromUtf8(raw));
                        }
                        emit orderFailed(ctxPtr->accountName, sym, msg);
                        emit logMessage(QStringLiteral("%1 UZX order error: %2")
                                            .arg(contextTag(ctxPtr->accountName), msg));
                        return;
                    }
                    const QString resp = QString::fromUtf8(raw);
                    emit logMessage(QStringLiteral("%1 UZX order response: %2")
                                        .arg(contextTag(ctxPtr->accountName),
                                             resp.isEmpty() ? QStringLiteral("<empty>") : resp));
                    bool accepted = true;
                    QJsonDocument uzxDoc = QJsonDocument::fromJson(raw);
                    if (!uzxDoc.isNull() && uzxDoc.isObject()) {
                        const QJsonObject obj = uzxDoc.object();
                        const int code = obj.value(QStringLiteral("code")).toInt(0);
                        if (code != 0) {
                            const QString msg = obj.value(QStringLiteral("msg"))
                                                    .toString(QStringLiteral("request error"));
                            emit orderFailed(ctxPtr->accountName, sym, msg);
                            emit logMessage(QStringLiteral("%1 UZX order rejected: %2 (code %3)")
                                                .arg(contextTag(ctxPtr->accountName))
                                                .arg(msg)
                                                .arg(code));
                            accepted = false;
                        }
                    }
                    if (!uzxDoc.isObject() && !resp.trimmed().isEmpty()) {
                        emit logMessage(QStringLiteral("%1 UZX response not JSON, assuming success")
                                            .arg(contextTag(ctxPtr->accountName)));
                    }
                    if (!accepted) {
                        return;
                    }
                    emit orderPlaced(ctxPtr->accountName, sym, side, price, quantity);
                    emit logMessage(QStringLiteral("%1 UZX order accepted: %2 %3 @ %4")
                                        .arg(contextTag(ctxPtr->accountName))
                                        .arg(side == OrderSide::Buy ? QStringLiteral("BUY")
                                                                    : QStringLiteral("SELL"))
                                        .arg(quantity, 0, 'f', 4)
                                        .arg(price, 0, 'f', 5));
                });
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("symbol"), sym);
    query.addQueryItem(QStringLiteral("side"),
                       side == OrderSide::Buy ? QStringLiteral("BUY") : QStringLiteral("SELL"));
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("LIMIT"));
    query.addQueryItem(QStringLiteral("timeInForce"), QStringLiteral("GTC"));
    query.addQueryItem(QStringLiteral("price"), QString::number(price, 'f', 8));
    query.addQueryItem(QStringLiteral("quantity"), QString::number(quantity, 'f', 8));
    query.addQueryItem(QStringLiteral("recvWindow"), QStringLiteral("5000"));
    query.addQueryItem(QStringLiteral("timestamp"),
                       QString::number(QDateTime::currentMSecsSinceEpoch()));

    QUrlQuery signedQuery = query;
    signedQuery.addQueryItem(QStringLiteral("signature"),
                             QString::fromLatin1(signPayload(query, ctx)));

    QNetworkRequest request = makePrivateRequest(QStringLiteral("/api/v3/order"),
                                                 signedQuery,
                                                 QByteArray(),
                                                 ctx);
    auto *reply = m_network.post(request, QByteArray());
    connect(reply,
            &QNetworkReply::finished,
            this,
            [this, reply, sym, side, price, quantity, ctxPtr = &ctx]() {
                const QNetworkReply::NetworkError err = reply->error();
                const int status =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray raw = reply->readAll();
                reply->deleteLater();
                if (err != QNetworkReply::NoError || status >= 400) {
                    QString msg = reply->errorString();
                    if (status >= 400) {
                        msg = QStringLiteral("HTTP %1: %2")
                                  .arg(status)
                                  .arg(QString::fromUtf8(raw));
                    }
                    emit orderFailed(ctxPtr->accountName, sym, msg);
                    emit logMessage(QStringLiteral("%1 Order error: %2")
                                        .arg(contextTag(ctxPtr->accountName), msg));
                    return;
                }
                emit logMessage(QStringLiteral("%1 MEXC order response: %2")
                                    .arg(contextTag(ctxPtr->accountName),
                                         raw.isEmpty() ? QStringLiteral("<empty>")
                                                       : QString::fromUtf8(raw)));
                const QJsonDocument doc = QJsonDocument::fromJson(raw);
                if (doc.isNull() || !doc.isObject()) {
                    emit orderFailed(ctxPtr->accountName, sym, tr("Invalid response"));
                    return;
                }
                const QJsonObject obj = doc.object();
                if (obj.contains(QStringLiteral("code"))
                    && obj.value(QStringLiteral("code")).toInt(0) != 0) {
                    const QString msg = obj.value(QStringLiteral("msg"))
                                             .toString(QStringLiteral("Unknown error"));
                    emit orderFailed(ctxPtr->accountName, sym, msg);
                    emit logMessage(QStringLiteral("%1 Order rejected: %2")
                                        .arg(contextTag(ctxPtr->accountName), msg));
                    return;
                }
                emit orderPlaced(ctxPtr->accountName, sym, side, price, quantity);
                emit logMessage(QStringLiteral("%1 Order accepted: %2 %3 @ %4")
                                    .arg(contextTag(ctxPtr->accountName))
                                    .arg(side == OrderSide::Buy ? QStringLiteral("BUY")
                                                                : QStringLiteral("SELL"))
                                    .arg(quantity, 0, 'f', 4)
                                    .arg(price, 0, 'f', 5));
            });
}

void TradeManager::cancelAllOrders(const QString &symbol, const QString &accountName)
{
    const QString sym = normalizedSymbol(symbol);
    const ConnectionStore::Profile profile = profileFromAccountName(accountName);
    Context &ctx = ensureContext(profile);
    if (!ensureCredentials(ctx)) {
        emit orderFailed(ctx.accountName, sym, tr("Missing credentials"));
        return;
    }
    if (ctx.state != ConnectionState::Connected) {
        emit orderFailed(ctx.accountName, sym, tr("Connect to the exchange first"));
        return;
    }
    if (profile == ConnectionStore::Profile::UzxSpot
        || profile == ConnectionStore::Profile::UzxSwap) {
        emit orderFailed(ctx.accountName, sym, tr("Cancel-all not implemented for UZX"));
        emit logMessage(QStringLiteral("%1 Cancel-all for UZX not supported yet")
                            .arg(contextTag(ctx.accountName)));
        return;
    }
    emit logMessage(QStringLiteral("%1 Cancel-all requested for %2")
                        .arg(contextTag(ctx.accountName), sym));
    ctx.pendingCancelSymbols.insert(sym);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("symbol"), sym);
    query.addQueryItem(QStringLiteral("recvWindow"), QStringLiteral("5000"));
    query.addQueryItem(QStringLiteral("timestamp"),
                       QString::number(QDateTime::currentMSecsSinceEpoch()));
    QUrlQuery signedQuery = query;
    signedQuery.addQueryItem(QStringLiteral("signature"),
                             QString::fromLatin1(signPayload(query, ctx)));

    QNetworkRequest req = makePrivateRequest(QStringLiteral("/api/v3/openOrders"),
                                             signedQuery,
                                             QByteArray(),
                                             ctx);
    auto *reply = m_network.sendCustomRequest(req, QByteArrayLiteral("DELETE"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, sym, ctxPtr = &ctx]() {
        const QNetworkReply::NetworkError err = reply->error();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError || status >= 400) {
            QString msg = reply->errorString();
            if (status >= 400) {
                msg = QStringLiteral("HTTP %1: %2").arg(status).arg(QString::fromUtf8(raw));
            }
            emit orderFailed(ctxPtr->accountName, sym, msg);
            emit logMessage(QStringLiteral("%1 Cancel all error: %2")
                                .arg(contextTag(ctxPtr->accountName), msg));
            return;
        }
        emit logMessage(QStringLiteral("%1 Cancel all sent for %2 (response: %3)")
                            .arg(contextTag(ctxPtr->accountName),
                                 sym,
                                 raw.isEmpty() ? QStringLiteral("<empty>")
                                               : QString::fromUtf8(raw)));
        QVector<OrderRecord> removedOrders;
        for (auto it = ctxPtr->activeOrders.begin(); it != ctxPtr->activeOrders.end();) {
            if (it.value().symbol == sym) {
                removedOrders.push_back(it.value());
                it = ctxPtr->activeOrders.erase(it);
            } else {
                ++it;
            }
        }
        for (const auto &record : removedOrders) {
            emit orderCanceled(ctxPtr->accountName, record.symbol, record.side, record.price);
        }
        if (!removedOrders.isEmpty()) {
            emitLocalOrderSnapshot(*ctxPtr, sym);
        }
    });
}
void TradeManager::handleOrderFill(Context &ctx,
                                   const QString &symbol,
                                   OrderSide side,
                                   double price,
                                   double quantity)
{
    const QString sym = normalizedSymbol(symbol);
    TradePosition &pos = ctx.positions[sym];
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
            TradePosition &newPos = ctx.positions[sym];
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
    emitPositionChanged(ctx, sym);
}

void TradeManager::emitPositionChanged(Context &ctx, const QString &symbol)
{
    emit positionChanged(ctx.accountName, symbol, ctx.positions.value(symbol));
}

QByteArray TradeManager::signPayload(const QUrlQuery &query, const Context &ctx) const
{
    const QByteArray payload = query.query(QUrl::FullyEncoded).toUtf8();
    return QMessageAuthenticationCode::hash(payload,
                                            ctx.credentials.secretKey.toUtf8(),
                                            QCryptographicHash::Sha256)
        .toHex();
}

QByteArray TradeManager::signUzxPayload(const QByteArray &body,
                                        const QString &method,
                                        const QString &path,
                                        const Context &ctx) const
{
    const QString ts = QString::number(QDateTime::currentSecsSinceEpoch());
    const QString base = ts + method.toUpper() + path + QString::fromUtf8(body);
    const QByteArray sig = QMessageAuthenticationCode::hash(base.toUtf8(),
                                                            ctx.credentials.secretKey.toUtf8(),
                                                            QCryptographicHash::Sha256)
                               .toBase64();
    QByteArray out;
    out.append(ts.toUtf8());
    out.append('\n');
    out.append(sig);
    return out;
}

QNetworkRequest TradeManager::makePrivateRequest(const QString &path,
                                                 const QUrlQuery &query,
                                                 const QByteArray &contentType,
                                                 const Context &ctx) const
{
    QUrl url(m_baseUrl + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkRequest req(url);
    if (!contentType.isEmpty()) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }
    req.setRawHeader("X-MEXC-APIKEY", ctx.credentials.apiKey.toUtf8());
    return req;
}

QNetworkRequest TradeManager::makeUzxRequest(const QString &path,
                                             const QByteArray &body,
                                             const QString &method,
                                             const Context &ctx) const
{
    QUrl url(m_uzxBaseUrl + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QByteArray tsSig = signUzxPayload(body, method, path, ctx);
    const QList<QByteArray> parts = tsSig.split('\n');
    const QByteArray ts = parts.value(0);
    const QByteArray sig = parts.value(1);
    req.setRawHeader("UZX-ACCESS-KEY", ctx.credentials.apiKey.toUtf8());
    req.setRawHeader("UZX-ACCESS-SIGN", sig);
    req.setRawHeader("UZX-ACCESS-TIMESTAMP", ts);
    req.setRawHeader("UZX-ACCESS-PASSPHRASE", ctx.credentials.passphrase.toUtf8());
    return req;
}

void TradeManager::requestListenKey(Context &ctx)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("timestamp"),
                       QString::number(QDateTime::currentMSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("recvWindow"), QStringLiteral("5000"));
    QUrlQuery signedQuery = query;
    signedQuery.addQueryItem(QStringLiteral("signature"),
                             QString::fromLatin1(signPayload(query, ctx)));
    QNetworkRequest request = makePrivateRequest(QStringLiteral("/api/v3/userDataStream"),
                                                 signedQuery,
                                                 QByteArrayLiteral("application/json"),
                                                 ctx);
    auto *reply = m_network.post(request, QByteArrayLiteral("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, ctxPtr = &ctx]() {
        const QNetworkReply::NetworkError err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            const QString message = raw.isEmpty() ? reply->errorString() : QString::fromUtf8(raw);
            resetConnection(*ctxPtr, message);
            emit logMessage(QStringLiteral("%1 Listen key request failed: %2")
                                .arg(contextTag(ctxPtr->accountName), message));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        const QJsonObject obj = doc.object();
        const QString listenKey = obj.value(QStringLiteral("listenKey")).toString();
        if (listenKey.isEmpty()) {
            resetConnection(*ctxPtr, QStringLiteral("Listen key missing"));
            emit logMessage(QStringLiteral("%1 Unexpected listen key payload: %2")
                                .arg(contextTag(ctxPtr->accountName), QString::fromUtf8(raw)));
            return;
        }
        emit logMessage(QStringLiteral("%1 Received listen key %2, opening private WS...")
                            .arg(contextTag(ctxPtr->accountName), listenKey));
        initializeWebSocket(*ctxPtr, listenKey);
    });
}

void TradeManager::initializeWebSocket(Context &ctx, const QString &listenKey)
{
    ctx.listenKey = listenKey;
    if (ctx.privateSocket.state() != QAbstractSocket::UnconnectedState) {
        ctx.closingSocket = true;
        ctx.privateSocket.close();
    }
    if (ctx.openOrdersTimer.isActive()) {
        ctx.openOrdersTimer.stop();
    }
    ctx.openOrdersPending = false;
    ctx.trackedSymbols.clear();
    QUrl url(QStringLiteral("wss://wbs-api.mexc.com/ws"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("listenKey"), listenKey);
    url.setQuery(query);
    emit logMessage(QStringLiteral("%1 Connecting to %2")
                        .arg(contextTag(ctx.accountName), url.toString(QUrl::RemoveUserInfo)));
    ctx.privateSocket.open(url);
}

void TradeManager::initializeUzxWebSocket(Context &ctx)
{
    if (ctx.privateSocket.state() != QAbstractSocket::UnconnectedState) {
        ctx.closingSocket = true;
        ctx.privateSocket.close();
    }
    QUrl url(QStringLiteral("wss://stream.uzx.com/notification/pri/ws"));
    emit logMessage(QStringLiteral("%1 Connecting to %2")
                        .arg(contextTag(ctx.accountName), url.toString(QUrl::RemoveUserInfo)));
    ctx.privateSocket.open(url);
}
void TradeManager::subscribePrivateChannels(Context &ctx)
{
    if (ctx.profile == ConnectionStore::Profile::UzxSwap
        || ctx.profile == ConnectionStore::Profile::UzxSpot) {
        subscribeUzxPrivate(ctx);
        return;
    }
    if (ctx.privateSocket.state() != QAbstractSocket::ConnectedState || ctx.hasSubscribed) {
        return;
    }
    const QStringList channels{QStringLiteral("spot@private.orders.v3.api.pb"),
                               QStringLiteral("spot@private.deals.v3.api.pb"),
                               QStringLiteral("spot@private.account.v3.api.pb")};
    QJsonObject payload;
    payload.insert(QStringLiteral("method"), QStringLiteral("SUBSCRIPTION"));
    payload.insert(QStringLiteral("params"), QJsonArray::fromStringList(channels));
    payload.insert(QStringLiteral("id"), 1);
    const QString message =
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    ctx.privateSocket.sendTextMessage(message);
    emit logMessage(QStringLiteral("%1 Subscribed to private channels.").arg(contextTag(ctx.accountName)));
    ctx.hasSubscribed = true;
    if (!ctx.openOrdersTimer.isActive()) {
        fetchOpenOrders(ctx);
        ctx.openOrdersTimer.start();
    }
}

void TradeManager::subscribeUzxPrivate(Context &ctx)
{
    if (ctx.privateSocket.state() != QAbstractSocket::ConnectedState) {
        return;
    }
    const QString path = QStringLiteral("/notification/pri/ws");
    const QString method = QStringLiteral("GET");
    const QString ts = QString::number(QDateTime::currentSecsSinceEpoch());
    const QString sign = QMessageAuthenticationCode::hash((ts + method + path).toUtf8(),
                                                          ctx.credentials.secretKey.toUtf8(),
                                                          QCryptographicHash::Sha256)
                             .toBase64();

    QJsonObject loginParams;
    loginParams.insert(QStringLiteral("type"), QStringLiteral("api"));
    loginParams.insert(QStringLiteral("api_key"), ctx.credentials.apiKey);
    loginParams.insert(QStringLiteral("api_timestamp"), ts);
    loginParams.insert(QStringLiteral("api_sign"), sign);
    loginParams.insert(QStringLiteral("api_passphrase"), ctx.credentials.passphrase);
    QJsonObject loginPayload;
    loginPayload.insert(QStringLiteral("event"), QStringLiteral("login"));
    loginPayload.insert(QStringLiteral("params"), loginParams);
    const QString loginMsg =
        QString::fromUtf8(QJsonDocument(loginPayload).toJson(QJsonDocument::Compact));
    ctx.privateSocket.sendTextMessage(loginMsg);
    emit logMessage(QStringLiteral("%1 Sent UZX login.").arg(contextTag(ctx.accountName)));
    ctx.hasSubscribed = false;
}

void TradeManager::sendListenKeyKeepAlive(Context &ctx)
{
    if (ctx.profile == ConnectionStore::Profile::UzxSwap
        || ctx.profile == ConnectionStore::Profile::UzxSpot) {
        return;
    }
    if (ctx.listenKey.isEmpty()) {
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("listenKey"), ctx.listenKey);
    QNetworkRequest request = makePrivateRequest(QStringLiteral("/api/v3/userDataStream"),
                                                 query,
                                                 QByteArray(),
                                                 ctx);
    auto *reply = m_network.put(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply, ctxPtr = &ctx]() {
        const QNetworkReply::NetworkError err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            const QString message = raw.isEmpty() ? reply->errorString() : QString::fromUtf8(raw);
            emit logMessage(QStringLiteral("%1 Keepalive failed: %2")
                                .arg(contextTag(ctxPtr->accountName), message));
            scheduleReconnect(*ctxPtr);
        } else {
            emit logMessage(QStringLiteral("%1 Listen key refreshed.")
                                .arg(contextTag(ctxPtr->accountName)));
        }
    });
}

void TradeManager::closeWebSocket(Context &ctx)
{
    if (ctx.reconnectTimer.isActive()) {
        ctx.reconnectTimer.stop();
    }
    ctx.keepAliveTimer.stop();
    ctx.wsPingTimer.stop();
    if (ctx.privateSocket.state() != QAbstractSocket::UnconnectedState) {
        ctx.closingSocket = true;
        ctx.privateSocket.close();
    }
}

void TradeManager::fetchOpenOrders(Context &ctx)
{
    if (ctx.profile == ConnectionStore::Profile::UzxSwap
        || ctx.profile == ConnectionStore::Profile::UzxSpot) {
        return;
    }
    if (ctx.openOrdersPending) {
        return;
    }
    ctx.openOrdersPending = true;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("recvWindow"), QStringLiteral("5000"));
    query.addQueryItem(QStringLiteral("timestamp"),
                       QString::number(QDateTime::currentMSecsSinceEpoch()));
    QUrlQuery signedQuery = query;
    signedQuery.addQueryItem(QStringLiteral("signature"),
                             QString::fromLatin1(signPayload(query, ctx)));
    QNetworkRequest req = makePrivateRequest(QStringLiteral("/api/v3/openOrders"),
                                             signedQuery,
                                             QByteArray(),
                                             ctx);
    auto *reply = m_network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, ctxPtr = &ctx]() {
        ctxPtr->openOrdersPending = false;
        const auto err = reply->error();
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            emit logMessage(QStringLiteral("%1 openOrders fetch failed: %2")
                                .arg(contextTag(ctxPtr->accountName),
                                     raw.isEmpty() ? reply->errorString() : QString::fromUtf8(raw)));
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isArray()) {
            return;
        }
        QHash<QString, QVector<DomWidget::LocalOrderMarker>> symbolMap;
        QSet<QString> newSymbols;
        QHash<QString, OrderRecord> fetchedOrders;
        QSet<QString> fetchedSymbols;
        const QJsonArray arr = doc.array();
        for (const auto &value : arr) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject order = value.toObject();
            const QString symbol = normalizedSymbol(order.value(QStringLiteral("symbol")).toString());
            if (symbol.isEmpty()) {
                continue;
            }
            const QString orderId =
                order.value(QStringLiteral("orderId")).toString().trimmed();
            if (orderId.isEmpty()) {
                continue;
            }
            const double price = order.value(QStringLiteral("price")).toString().toDouble();
            const double origQty = order.value(QStringLiteral("origQty")).toString().toDouble();
            const double execQty = order.value(QStringLiteral("executedQty")).toString().toDouble();
            const double remainQty = origQty - execQty;
            if (price <= 0.0 || remainQty <= 0.0) {
                continue;
            }
            DomWidget::LocalOrderMarker marker;
            marker.price = price;
            marker.quantity = std::abs(price * remainQty);
            const QString side = order.value(QStringLiteral("side")).toString();
            marker.side = side.compare(QStringLiteral("SELL"), Qt::CaseInsensitive) == 0 ? OrderSide::Sell
                                                                                         : OrderSide::Buy;
            marker.createdMs = order.value(QStringLiteral("time")).toVariant().toLongLong();
            symbolMap[symbol].push_back(marker);
            newSymbols.insert(symbol);
            fetchedSymbols.insert(symbol);

            OrderRecord record;
            record.symbol = symbol;
            record.price = price;
            record.quantityNotional = std::abs(price * remainQty);
            record.side = marker.side;
            record.createdMs = marker.createdMs;
            fetchedOrders.insert(orderId, record);
        }
        QList<OrderRecord> removedOrders;
        for (auto it = ctxPtr->activeOrders.constBegin(); it != ctxPtr->activeOrders.constEnd();
             ++it) {
            if (!fetchedOrders.contains(it.key())) {
                removedOrders.push_back(it.value());
            }
        }
        ctxPtr->activeOrders = fetchedOrders;
        for (const auto &record : removedOrders) {
            emit orderCanceled(ctxPtr->accountName, record.symbol, record.side, record.price);
        }
        for (auto it = ctxPtr->pendingCancelSymbols.begin(); it != ctxPtr->pendingCancelSymbols.end();) {
            if (!fetchedSymbols.contains(*it)) {
                it = ctxPtr->pendingCancelSymbols.erase(it);
            } else {
                ++it;
            }
        }
        QSet<QString> allSymbols = ctxPtr->trackedSymbols;
        allSymbols.unite(newSymbols);
        for (const QString &symbol : allSymbols) {
            QVector<DomWidget::LocalOrderMarker> output;
            if (!ctxPtr->pendingCancelSymbols.contains(symbol)) {
                output = symbolMap.value(symbol);
            }
            emit localOrdersUpdated(ctxPtr->accountName, symbol, output);
        }
        ctxPtr->trackedSymbols = newSymbols;
    });
}

void TradeManager::resetConnection(Context &ctx, const QString &reason)
{
    closeWebSocket(ctx);
    clearLocalOrderSnapshots(ctx);
    ctx.hasSubscribed = false;
    ctx.listenKey.clear();
    setState(ctx, ConnectionState::Error, reason);
    scheduleReconnect(ctx);
}

void TradeManager::scheduleReconnect(Context &ctx)
{
    if (ctx.reconnectTimer.isActive()) {
        return;
    }
    ctx.reconnectTimer.start();
}
void TradeManager::processPrivateDeal(Context &ctx, const QByteArray &body, const QString &symbol)
{
    if (symbol.isEmpty()) {
        emit logMessage(QStringLiteral("%1 Private deal missing symbol.").arg(contextTag(ctx.accountName)));
        return;
    }
    PrivateDealEvent event;
    if (!parsePrivateDealBody(body, event)) {
        emit logMessage(QStringLiteral("%1 Failed to parse private deal.").arg(contextTag(ctx.accountName)));
        return;
    }
    if (event.quantity <= 0.0 || event.price <= 0.0) {
        return;
    }
    const QString sym = normalizedSymbol(symbol);
    const OrderSide side = event.tradeType == 1 ? OrderSide::Buy : OrderSide::Sell;
    handleOrderFill(ctx, sym, side, event.price, event.quantity);
    emit logMessage(QStringLiteral("%1 Deal %2 %3 %4 @ %5 (order %6)")
                        .arg(contextTag(ctx.accountName))
                        .arg(sym)
                        .arg(side == OrderSide::Buy ? QStringLiteral("BUY")
                                                    : QStringLiteral("SELL"))
                        .arg(event.quantity, 0, 'f', 8)
                        .arg(event.price, 0, 'f', 8)
                        .arg(event.orderId));
}

void TradeManager::processPrivateOrder(Context &ctx,
                                       const QByteArray &body,
                                       const QString &symbol)
{
    PrivateOrderEvent event;
    if (!parsePrivateOrderBody(body, event)) {
        emit logMessage(QStringLiteral("%1 Failed to parse private order payload.")
                            .arg(contextTag(ctx.accountName)));
        return;
    }
    const QString identifier = !event.id.isEmpty() ? event.id : event.clientId;
    const QString normalizedSym = normalizedSymbol(symbol);
    emit logMessage(QStringLiteral("%1 Order %2 (%3): status=%4 remain=%5 cumQty=%6 @avg %7")
                        .arg(contextTag(ctx.accountName))
                        .arg(identifier)
                        .arg(symbol)
                        .arg(statusText(event.status))
                        .arg(event.remainQuantity, 0, 'f', 8)
                        .arg(event.cumulativeQuantity, 0, 'f', 8)
                        .arg(event.avgPrice, 0, 'f', 8));
    const QString orderId = !event.id.isEmpty() ? event.id : event.clientId;
    const OrderSide side = event.tradeType == 1 ? OrderSide::Buy : OrderSide::Sell;
    if (!orderId.isEmpty() && !normalizedSym.isEmpty()) {
        const double price = event.price;
        const double remain = event.remainQuantity;
        const double notional = price > 0.0 && remain > 0.0 ? price * remain : 0.0;
        if (notional > 0.0) {
            OrderRecord record;
            record.symbol = normalizedSym;
            record.side = side;
            record.price = price;
            record.quantityNotional = notional;
            record.createdMs = event.createTime > 0 ? event.createTime : QDateTime::currentMSecsSinceEpoch();
            ctx.activeOrders.insert(orderId, record);
        } else {
            ctx.activeOrders.remove(orderId);
        }
        emitLocalOrderSnapshot(ctx, normalizedSym);
    }
    if (event.status == 2 || event.status == 4 || event.status == 5
        || event.remainQuantity <= 0.0) {
        ctx.pendingCancelSymbols.remove(normalizedSym);
        emit orderCanceled(ctx.accountName, normalizedSym, side, event.price);
    }
}

void TradeManager::processPrivateAccount(Context &ctx, const QByteArray &body)
{
    PrivateAccountEvent event;
    if (!parsePrivateAccountBody(body, event)) {
        emit logMessage(QStringLiteral("%1 Failed to parse private account payload.")
                            .arg(contextTag(ctx.accountName)));
        return;
    }
    emit logMessage(QStringLiteral("%1 Balance %2: available=%3 frozen=%4 (%5)")
                        .arg(contextTag(ctx.accountName))
                        .arg(event.asset)
                        .arg(event.balance, 0, 'f', 8)
                        .arg(event.frozen, 0, 'f', 8)
                        .arg(event.changeType));
}

void TradeManager::emitLocalOrderSnapshot(Context &ctx, const QString &symbol)
{
    const QString normalized = normalizedSymbol(symbol);
    QVector<DomWidget::LocalOrderMarker> markers;
    markers.reserve(ctx.activeOrders.size());
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const auto &record : ctx.activeOrders) {
        if (record.symbol != normalized || record.price <= 0.0 || record.quantityNotional <= 0.0) {
            continue;
        }
        DomWidget::LocalOrderMarker marker;
        marker.price = record.price;
        marker.quantity = record.quantityNotional;
        marker.side = record.side;
        marker.createdMs = record.createdMs > 0 ? record.createdMs : nowMs;
        markers.push_back(marker);
    }
    emit localOrdersUpdated(ctx.accountName, normalized, markers);
}

void TradeManager::clearLocalOrderSnapshots(Context &ctx)
{
    if (ctx.activeOrders.isEmpty()) {
        return;
    }
    QSet<QString> symbols;
    symbols.reserve(ctx.activeOrders.size());
    for (const auto &record : ctx.activeOrders) {
        if (!record.symbol.isEmpty()) {
            symbols.insert(record.symbol);
        }
    }
    ctx.activeOrders.clear();
    for (const QString &symbol : symbols) {
        emit localOrdersUpdated(ctx.accountName, symbol, {});
    }
}

TradeManager::Context *TradeManager::contextForProfile(ConnectionStore::Profile profile) const
{
    return m_contexts.value(profile, nullptr);
}

TradeManager::Context &TradeManager::ensureContext(ConnectionStore::Profile profile) const
{
    if (Context *ctx = contextForProfile(profile)) {
        return *ctx;
    }
    auto *ctx = new Context;
    ctx->profile = profile;
    ctx->accountName = defaultAccountName(profile);

    auto *self = const_cast<TradeManager *>(this);
    ctx->keepAliveTimer.setParent(self);
    ctx->wsPingTimer.setParent(self);
    ctx->reconnectTimer.setParent(self);
    ctx->privateSocket.setParent(self);

    ctx->keepAliveTimer.setInterval(25 * 60 * 1000);
    self->connect(&ctx->keepAliveTimer, &QTimer::timeout, self, [self, ctx]() {
        self->sendListenKeyKeepAlive(*ctx);
    });

    ctx->wsPingTimer.setInterval(45 * 1000);
    self->connect(&ctx->wsPingTimer, &QTimer::timeout, self, [ctx]() {
        if (ctx->privateSocket.isValid()) {
            ctx->privateSocket.ping();
        }
    });

    ctx->reconnectTimer.setSingleShot(true);
    ctx->reconnectTimer.setInterval(3000);
    self->connect(&ctx->reconnectTimer, &QTimer::timeout, self, [self, ctx]() {
        if (ctx->state == ConnectionState::Disconnected || ctx->state == ConnectionState::Error) {
            emit self->logMessage(QStringLiteral("%1 Reconnecting private WebSocket...")
                                      .arg(contextTag(ctx->accountName)));
            self->connectToExchange(ctx->profile);
        }
    });

    ctx->openOrdersTimer.setSingleShot(false);
    ctx->openOrdersTimer.setInterval(4000);
    self->connect(&ctx->openOrdersTimer, &QTimer::timeout, self, [self, ctx]() {
        if (ctx->profile == ConnectionStore::Profile::UzxSwap
            || ctx->profile == ConnectionStore::Profile::UzxSpot) {
            return;
        }
        self->fetchOpenOrders(*ctx);
    });

    self->connect(&ctx->privateSocket, &QWebSocket::connected, self, [self, ctx]() {
        if (ctx->profile == ConnectionStore::Profile::UzxSwap
            || ctx->profile == ConnectionStore::Profile::UzxSpot) {
            emit self->logMessage(QStringLiteral("%1 UZX private WebSocket connected.")
                                      .arg(contextTag(ctx->accountName)));
            self->subscribeUzxPrivate(*ctx);
            if (ctx->reconnectTimer.isActive()) {
                ctx->reconnectTimer.stop();
            }
            self->setState(*ctx, ConnectionState::Connecting, tr("Authenticating..."));
            return;
        }
        emit self->logMessage(QStringLiteral("%1 Private WebSocket connected.").arg(contextTag(ctx->accountName)));
        self->subscribePrivateChannels(*ctx);
        self->sendListenKeyKeepAlive(*ctx);
        if (!ctx->keepAliveTimer.isActive()) {
            ctx->keepAliveTimer.start();
        }
        if (!ctx->wsPingTimer.isActive()) {
            ctx->wsPingTimer.start();
        }
        if (ctx->reconnectTimer.isActive()) {
            ctx->reconnectTimer.stop();
        }
        self->setState(*ctx, ConnectionState::Connected, tr("Connected to private WebSocket"));
    });

    self->connect(&ctx->privateSocket,
                  &QWebSocket::disconnected,
                  self,
                  [self, ctx]() {
                      ctx->keepAliveTimer.stop();
                      ctx->wsPingTimer.stop();
                      if (ctx->closingSocket) {
                          ctx->closingSocket = false;
                          emit self->logMessage(QStringLiteral("%1 Private WebSocket closed.")
                                                    .arg(contextTag(ctx->accountName)));
                          return;
                      }
                      emit self->logMessage(QStringLiteral(
                                                "%1 Private WebSocket disconnected unexpectedly. code=%2 reason=%3")
                                                .arg(contextTag(ctx->accountName))
                                                .arg(static_cast<int>(ctx->privateSocket.closeCode()))
                                                .arg(ctx->privateSocket.closeReason()));
                      self->setState(*ctx, ConnectionState::Error, tr("WebSocket disconnected"));
                      self->scheduleReconnect(*ctx);
                  });

    self->connect(&ctx->privateSocket,
                  QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
                  self,
                  [self, ctx](QAbstractSocket::SocketError) {
                      if (ctx->closingSocket) {
                          return;
                      }
                      emit self->logMessage(QStringLiteral("%1 WebSocket error: %2")
                                                .arg(contextTag(ctx->accountName))
                                                .arg(ctx->privateSocket.errorString()));
                      if (ctx->state != ConnectionState::Error) {
                           self->setState(*ctx, ConnectionState::Error, ctx->privateSocket.errorString());
                      }
                      self->scheduleReconnect(*ctx);
                  });
    self->connect(&ctx->privateSocket,
                  &QWebSocket::textMessageReceived,
                  self,
                  [self, ctx](const QString &message) {
                      const auto profile = ctx->profile;
                      const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
                      if (!doc.isObject()) {
                          emit self->logMessage(QStringLiteral("%1 WS text: %2")
                                              .arg(contextTag(ctx->accountName), message));
                          return;
                      }
                      const QJsonObject obj = doc.object();
                      if (profile == ConnectionStore::Profile::UzxSwap
                          || profile == ConnectionStore::Profile::UzxSpot) {
                          if (obj.contains(QStringLiteral("ping"))) {
                              QJsonObject pong;
                              pong.insert(QStringLiteral("pong"), obj.value(QStringLiteral("ping")));
                              ctx->privateSocket.sendTextMessage(
                                  QString::fromUtf8(
                                      QJsonDocument(pong).toJson(QJsonDocument::Compact)));
                              return;
                          }
                          const QString event = obj.value(QStringLiteral("event")).toString();
                          if (event.compare(QStringLiteral("login"), Qt::CaseInsensitive) == 0) {
                              const QString status = obj.value(QStringLiteral("status")).toString();
                              if (status.compare(QStringLiteral("success"), Qt::CaseInsensitive) != 0) {
                                  const QString msg = obj.value(QStringLiteral("msg"))
                                                            .toString(
                                                                obj.value(QStringLiteral("message"))
                                                                    .toString(status));
                                  emit self->logMessage(QStringLiteral("%1 UZX login failed: %2")
                                                      .arg(contextTag(ctx->accountName), msg));
                                  self->setState(*ctx, ConnectionState::Error, msg);
                                  ctx->hasSubscribed = false;
                                  self->closeWebSocket(*ctx);
                              } else {
                                  emit self->logMessage(QStringLiteral("%1 UZX login response: %2")
                                                      .arg(contextTag(ctx->accountName), message));
                                  QJsonObject subParams;
                                  subParams.insert(QStringLiteral("biz"), QStringLiteral("private"));
                                  subParams.insert(QStringLiteral("type"),
                                                   profile == ConnectionStore::Profile::UzxSpot
                                                       ? QStringLiteral("order.spot")
                                                       : QStringLiteral("order.swap"));
                                  QJsonObject subPayload;
                                  subPayload.insert(QStringLiteral("event"), QStringLiteral("sub"));
                                  subPayload.insert(QStringLiteral("params"), subParams);
                                  subPayload.insert(QStringLiteral("zip"), false);
                                  ctx->privateSocket.sendTextMessage(
                                      QString::fromUtf8(QJsonDocument(subPayload)
                                                            .toJson(QJsonDocument::Compact)));
                                  emit self->logMessage(QStringLiteral(
                                                      "%1 Subscribed to UZX private order updates.")
                                                      .arg(contextTag(ctx->accountName)));
                                  ctx->hasSubscribed = true;
                                  self->setState(*ctx,
                                           ConnectionState::Connected,
                                           QStringLiteral("UZX authenticated"));
                              }
                              return;
                          }
                          const QString type = obj.value(QStringLiteral("type")).toString();
                          const bool isOrder = type == QStringLiteral("order.swap")
                                               || type == QStringLiteral("order.spot");
                          if (isOrder) {
                              const QString name = obj.value(QStringLiteral("name")).toString();
                              const QJsonObject data = obj.value(QStringLiteral("data")).toObject();
                              const double price = data.value(QStringLiteral("price"))
                                                       .toString()
                                                       .toDouble();
                              const double filled = data.value(QStringLiteral("deal_number")).toDouble();
                              emit self->logMessage(QStringLiteral("%1 UZX order update %2: %3")
                                                  .arg(contextTag(ctx->accountName))
                                                  .arg(name,
                                                       QString::fromUtf8(
                                                           QJsonDocument(data)
                                                               .toJson(QJsonDocument::Compact))));
                              if (filled > 0.0 && price > 0.0) {
                                  const int sideFlag =
                                      data.value(QStringLiteral("order_buy_or_sell")).toInt(1);
                                  const OrderSide side = sideFlag == 2 ? OrderSide::Sell : OrderSide::Buy;
                                  self->handleOrderFill(*ctx, name, side, price, filled);
                              }
                              if (data.contains(QStringLiteral("un_filled_number"))
                                  && data.value(QStringLiteral("un_filled_number")).toDouble() <= 0.0) {
                                  const int sideFlag =
                                      data.value(QStringLiteral("order_buy_or_sell")).toInt(1);
                                  const OrderSide side = sideFlag == 2 ? OrderSide::Sell : OrderSide::Buy;
                                  emit self->orderCanceled(ctx->accountName,
                                                           normalizedSymbol(name),
                                                           side,
                                                           price);
                              }
                              return;
                          }
                          emit self->logMessage(QStringLiteral("%1 UZX WS: %2")
                                              .arg(contextTag(ctx->accountName), message));
                          return;
                      }
                      const QString method = obj.value(QStringLiteral("method")).toString().toUpper();
                      if (method == QStringLiteral("PING")) {
                          ctx->privateSocket.sendTextMessage(QStringLiteral("{\"method\":\"PONG\"}"));
                          return;
                      }
                      if (obj.contains(QStringLiteral("code"))) {
                          emit self->logMessage(QStringLiteral("%1 WS event: %2")
                                              .arg(contextTag(ctx->accountName), message));
                      }
                  });

    self->connect(&ctx->privateSocket,
                  &QWebSocket::binaryMessageReceived,
                  self,
                  [self, ctx](const QByteArray &payload) {
                      if (ctx->profile == ConnectionStore::Profile::UzxSwap
                          || ctx->profile == ConnectionStore::Profile::UzxSpot) {
                          return;
                      }
                      PushMessage message;
                      if (!parsePushMessage(payload, message)) {
                          emit self->logMessage(QStringLiteral("%1 Failed to decode private WS payload.")
                                                    .arg(contextTag(ctx->accountName)));
                          return;
                      }
                      switch (message.type) {
                      case PushBodyType::PrivateDeals:
                          self->processPrivateDeal(*ctx, message.body, message.symbol);
                          break;
                      case PushBodyType::PrivateOrders:
                          self->processPrivateOrder(*ctx, message.body, message.symbol);
                          break;
                      case PushBodyType::PrivateAccount:
                          self->processPrivateAccount(*ctx, message.body);
                          break;
                      default:
                          break;
                      }
                  });

    m_contexts.insert(profile, ctx);
    return *ctx;
}

QString TradeManager::defaultAccountName(ConnectionStore::Profile profile) const
{
    switch (profile) {
    case ConnectionStore::Profile::MexcFutures:
        return QStringLiteral("MEXC Futures");
    case ConnectionStore::Profile::UzxSwap:
        return QStringLiteral("UZX Swap");
    case ConnectionStore::Profile::UzxSpot:
        return QStringLiteral("UZX Spot");
    case ConnectionStore::Profile::MexcSpot:
    default:
        return QStringLiteral("MEXC Spot");
    }
}

QString TradeManager::profileKey(ConnectionStore::Profile profile) const
{
    switch (profile) {
    case ConnectionStore::Profile::MexcFutures:
        return QStringLiteral("mexcFutures");
    case ConnectionStore::Profile::UzxSwap:
        return QStringLiteral("uzxSwap");
    case ConnectionStore::Profile::UzxSpot:
        return QStringLiteral("uzxSpot");
    case ConnectionStore::Profile::MexcSpot:
    default:
        return QStringLiteral("mexcSpot");
    }
}

void TradeManager::setState(Context &ctx, ConnectionState state, const QString &message)
{
    if (ctx.state == state && message.isEmpty()) {
        return;
    }
    ctx.state = state;
    emit connectionStateChanged(ctx.profile, state, message);
}

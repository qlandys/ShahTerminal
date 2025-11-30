#pragma once

#include <QString>
#include <QMetaType>

enum class OrderSide {
    Buy,
    Sell
};

struct MexcCredentials {
    QString apiKey;
    QString secretKey;
    bool saveSecret = false;
    bool viewOnly = false;
    bool autoConnect = true;
};

struct TradePosition {
    bool hasPosition = false;
    OrderSide side = OrderSide::Buy;
    double averagePrice = 0.0;
    double quantity = 0.0;
    double realizedPnl = 0.0;
};

Q_DECLARE_METATYPE(MexcCredentials)
Q_DECLARE_METATYPE(TradePosition)

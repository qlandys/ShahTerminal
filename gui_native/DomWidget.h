#pragma once

#include <QWidget>
#include <QVector>
#include <QString>
#include "DomTypes.h"
#include "TradeTypes.h"

class QMouseEvent;
class QEvent;

struct DomLevel {
    double price = 0.0;
    double bidQty = 0.0;
    double askQty = 0.0;
};

struct DomSnapshot {
    QVector<DomLevel> levels;
    double bestBid = 0.0;
    double bestAsk = 0.0;
    double tickSize = 0.0;
};

struct DomStyle {
    QColor background = QColor("#202020");
    QColor text = QColor("#f0f0f0");
    QColor bid = QColor(170, 255, 190);
    QColor ask = QColor(255, 180, 190);
    QColor grid = QColor("#303030");
};

class DomWidget : public QWidget {
    Q_OBJECT

public:
    struct LocalOrderMarker
    {
        double price = 0.0;
        double quantity = 0.0;
        OrderSide side = OrderSide::Buy;
        qint64 createdMs = 0;
        QString orderId;
    };

    explicit DomWidget(QWidget *parent = nullptr);

    void updateSnapshot(const DomSnapshot &snapshot);
    void setStyle(const DomStyle &style);
    void setInitialCenterPrice(double price);
    void centerToSpread();
    int rowHeight() const { return m_rowHeight; }
    void setRowHeight(int h);
    void setVolumeHighlightRules(const QVector<VolumeHighlightRule> &rules);
    void setTradePosition(const TradePosition &position);
    void setLocalOrders(const QVector<LocalOrderMarker> &orders);

signals:
    void rowClicked(Qt::MouseButton button, int row, double price, double bidQty, double askQty);
    void rowHovered(int row, double price, double bidQty, double askQty);
    void hoverInfoChanged(int row, double price, const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    DomSnapshot m_snapshot;
    DomStyle m_style;
    QVector<VolumeHighlightRule> m_volumeRules;
    int m_hoverRow = -1;
    QString m_hoverInfoText;
    double m_initialCenterPrice = 0.0;
    bool m_hasInitialCenter = false;
    int m_rowHeight = 12;
    TradePosition m_position;
    int m_infoAreaHeight = 26;
    QVector<LocalOrderMarker> m_localOrders;

    void updateHoverInfo(int row);
    double cumulativeNotionalForRow(int row) const;
    int rowForPrice(double price) const;
};

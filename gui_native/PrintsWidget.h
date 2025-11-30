#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QHash>
#include <QBasicTimer>

struct PrintItem {
    double price = 0.0;
    double qty = 0.0;
    bool buy = true;
    int rowHint = -1;
};

class PrintsWidget : public QWidget {
    Q_OBJECT
public:
    explicit PrintsWidget(QWidget *parent = nullptr);

    void setPrints(const QVector<PrintItem> &items);
    void setLadderPrices(const QVector<double> &prices, int rowHeight, double tickSize);
    void setRowHeightOnly(int rowHeight);

public slots:
    void setHoverInfo(int row, double price, const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QString makeKey(const PrintItem &item) const;
    int rowForPrice(double price) const;
    int applyRowOffset(int row) const;
    void calibrateRowOffset(int domRow, int priceRow);

    QVector<PrintItem> m_items;
    QVector<double> m_prices;
    QHash<double, int> m_priceToRow;
    int m_rowHeight = 20;
    QHash<QString, double> m_spawnProgress;
    QBasicTimer m_animTimer;
    int m_hoverRow = -1;
    double m_hoverPrice = 0.0;
    bool m_hoverPriceValid = false;
    QString m_hoverText;
    double m_tickSize = 0.0;
    bool m_descending = true;
    double m_firstPrice = 0.0;
    int m_rowOffset = 0;
    bool m_rowOffsetValid = false;
    // Keep prints widget height aligned with DomWidget (which has an extra info area at the bottom).
    static constexpr int kDomInfoAreaHeight = 26;
};

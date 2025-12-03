#include "DomWidget.h"

#include <QAbstractScrollArea>
#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QScrollBar>
#include <algorithm>
#include <limits>
#include <cmath>
#include <utility>

namespace {
QString formatPriceForDisplay(double price, int precision)
{
    const QString base = QString::number(price, 'f', precision);
    const int dot = base.indexOf('.');
    if (dot < 0) {
        return base;
    }

    const QString intPart = base.left(dot);
    const QString frac = base.mid(dot + 1);
    if (intPart == QLatin1String("0")) {
        int zeroCount = 0;
        while (zeroCount < frac.size() && frac.at(zeroCount) == QLatin1Char('0')) {
            ++zeroCount;
        }
        if (zeroCount > 0) {
            QString remainder = frac.mid(zeroCount);
            if (remainder.isEmpty()) {
                remainder = QStringLiteral("0");
            }
            return QStringLiteral("(%1)%2").arg(zeroCount).arg(remainder);
        }
    }
    return base;
}

QString formatQty(double v)
{
    const double av = std::abs(v);
    if (av >= 1000000.0) {
        return QString::number(av / 1000000.0, 'f', av >= 10000000.0 ? 0 : 1) + QStringLiteral("M");
    }
    if (av >= 1000.0) {
        return QString::number(av / 1000.0, 'f', av >= 10000.0 ? 0 : 1) + QStringLiteral("K");
    }
    if (av >= 100.0) {
        return QString::number(av, 'f', 0);
    }
    return QString::number(av, 'f', 1);
}

QString formatValueShort(double v)
{
    const double av = std::abs(v);
    QString suffix;
    double value = av;
    if (av >= 1000000000.0) {
        value = av / 1000000000.0;
        suffix = QStringLiteral("B");
    } else if (av >= 1000000.0) {
        value = av / 1000000.0;
        suffix = QStringLiteral("M");
    } else if (av >= 1000.0) {
        value = av / 1000.0;
        suffix = QStringLiteral("K");
    }
    const int precision = value >= 10.0 ? 1 : 2;
    const QString text = QString::number(value, 'f', precision);
    return suffix.isEmpty() ? text : text + suffix;
}


bool percentFromReference(double price, double bestBid, double bestAsk, double &outPercent)
{
    if (bestBid > 0.0 && price <= bestBid) {
        outPercent = (bestBid - price) / bestBid * 100.0;
        return true;
    }
    if (bestAsk > 0.0 && price >= bestAsk) {
        outPercent = (price - bestAsk) / bestAsk * 100.0;
        return true;
    }
    if (bestAsk > 0.0 && price < bestAsk) {
        outPercent = -((bestAsk - price) / bestAsk * 100.0);
        return true;
    }
    if (bestBid > 0.0 && price > bestBid) {
        outPercent = -((price - bestBid) / bestBid * 100.0);
        return true;
    }
    return false;
}

double priceTolerance(double tick)
{
    if (tick > 0.0) {
        return std::max(1e-8, tick * 0.25);
    }
    return 1e-8;
}
} // namespace

DomWidget::DomWidget(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setMouseTracking(true);
    // Prefer JetBrains Mono for ladder, fall back to current app font.
    QFont f = font();
    f.setFamily(QStringLiteral("JetBrains Mono"));
    f.setPointSize(9);
    setFont(f);
}

void DomWidget::updateSnapshot(const DomSnapshot &snapshot)
{
    m_snapshot = snapshot;
    const int rows = m_snapshot.levels.size();
    const int rowHeight = m_rowHeight;
    const int totalHeight = std::max(rows * rowHeight + m_infoAreaHeight, minimumSizeHint().height());
    setMinimumHeight(totalHeight);
    setMaximumHeight(totalHeight);
    updateGeometry();

    if (m_hasInitialCenter && !m_snapshot.levels.isEmpty()) {
        QWidget *w = parentWidget();
        QAbstractScrollArea *area = nullptr;
        while (w && !area) {
            area = qobject_cast<QAbstractScrollArea *>(w);
            w = w->parentWidget();
        }
        if (area) {
            QScrollBar *sb = area->verticalScrollBar();
            if (sb) {
                int bestRow = 0;
                double bestDist = std::numeric_limits<double>::max();
                for (int i = 0; i < m_snapshot.levels.size(); ++i) {
                    double d = std::abs(m_snapshot.levels[i].price - m_initialCenterPrice);
                    if (d < bestDist) {
                        bestDist = d;
                        bestRow = i;
                    }
                }
                const int centerPixel = bestRow * rowHeight + rowHeight / 2;
                const int viewportHeight = area->viewport()->height();
                int value = centerPixel - viewportHeight / 2;
                value = std::max(sb->minimum(), std::min(sb->maximum(), value));
                sb->setValue(value);
            }
        }
        m_hasInitialCenter = false;
    }

    if (m_hoverRow >= rows) {
        m_hoverRow = -1;
        m_hoverInfoText.clear();
        emit hoverInfoChanged(-1, 0.0, QString());
    } else if (m_hoverRow >= 0) {
        updateHoverInfo(m_hoverRow);
    }

    update(); // schedule repaint
}

void DomWidget::setStyle(const DomStyle &style)
{
    m_style = style;
    update();
}

void DomWidget::setRowHeight(int h)
{
    int clamped = std::clamp(h, 10, 40);
    if (clamped == m_rowHeight) {
        return;
    }
    m_rowHeight = clamped;
    if (!m_snapshot.levels.isEmpty()) {
        updateSnapshot(m_snapshot);
    } else {
        update();
    }
}

void DomWidget::setInitialCenterPrice(double price)
{
    m_initialCenterPrice = price;
    m_hasInitialCenter = true;
}

void DomWidget::centerToSpread()
{
    if (m_snapshot.levels.isEmpty()) {
        return;
    }

    double bestBid = 0.0;
    double bestAsk = 0.0;
    bool hasBid = false;
    bool hasAsk = false;

    for (const auto &lvl : m_snapshot.levels) {
        if (lvl.bidQty > 0.0) {
            if (!hasBid || lvl.price > bestBid) {
                bestBid = lvl.price;
                hasBid = true;
            }
        }
        if (lvl.askQty > 0.0) {
            if (!hasAsk || lvl.price < bestAsk) {
                bestAsk = lvl.price;
                hasAsk = true;
            }
        }
    }

    double centerPrice = 0.0;
    if (hasBid && hasAsk) {
        centerPrice = (bestBid + bestAsk) * 0.5;
    } else if (hasBid) {
        centerPrice = bestBid;
    } else if (hasAsk) {
        centerPrice = bestAsk;
    } else {
        return;
    }

    QWidget *w = parentWidget();
    QAbstractScrollArea *area = nullptr;
    while (w && !area) {
        area = qobject_cast<QAbstractScrollArea *>(w);
        w = w->parentWidget();
    }
    if (!area) {
        return;
    }

    QScrollBar *sb = area->verticalScrollBar();
    if (!sb) {
        return;
    }

    const int rows = m_snapshot.levels.size();
    if (rows <= 0) {
        return;
    }

    int bestRow = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (int i = 0; i < rows; ++i) {
        double d = std::abs(m_snapshot.levels[i].price - centerPrice);
        if (d < bestDist) {
            bestDist = d;
            bestRow = i;
        }
    }

    const int centerPixel = bestRow * m_rowHeight + m_rowHeight / 2;
    const int viewportHeight = area->viewport()->height();
    int value = centerPixel - viewportHeight / 2;
    value = std::max(sb->minimum(), std::min(sb->maximum(), value));
    sb->setValue(value);
}

void DomWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Background
    p.fillRect(rect(), m_style.background);

    if (m_snapshot.levels.isEmpty()) {
        return;
    }

    const int w = width();
    const QRect clipRect = event->rect();
    const int rows = m_snapshot.levels.size();
    if (rows <= 0) {
        return;
    }

    const double rowHeight = static_cast<double>(m_rowHeight);
    const double bestPriceTolerance = priceTolerance(m_snapshot.tickSize);
    QFontMetrics fm(font());

    // Reserve fixed-width price column with a small trailing margin.
    const int priceColWidth = std::max(48, fm.horizontalAdvance(QStringLiteral("(3)00000")) + 4);
    const int priceRight = w - 1;
    const int priceLeft = std::max(0, priceRight - priceColWidth);

    const int firstRow = std::max(0, static_cast<int>(clipRect.top() / rowHeight));
    const int lastRow = std::min(rows - 1, static_cast<int>(clipRect.bottom() / rowHeight));

    // Grid vertical lines for price column borders.
    p.setPen(m_style.grid);
    if (priceLeft > 0) {
        p.drawLine(QPoint(priceLeft, 0), QPoint(priceLeft, height()));
    }
    p.drawLine(QPoint(priceRight, 0), QPoint(priceRight, height()));

    for (int i = firstRow; i <= lastRow; ++i) {
        const DomLevel &lvl = m_snapshot.levels[i];

        const int y = static_cast<int>(i * rowHeight);
        const int rowIntHeight = std::max(1, static_cast<int>(rowHeight));
        const QRect rowRect(0, y, w, rowIntHeight);
        const QRect bookRect(0, y, std::max(0, priceLeft + 1), rowIntHeight);
        const QRect priceRect(priceLeft, y, priceColWidth, rowIntHeight);

        // Simple side coloring based on which side is larger.
        const double bidQty = lvl.bidQty;
        const double askQty = lvl.askQty;
        const bool hasBid = bidQty > 0.0;
        const bool hasAsk = askQty > 0.0;
        const bool isBestBidRow = m_snapshot.bestBid > 0.0 && std::abs(lvl.price - m_snapshot.bestBid) <= bestPriceTolerance;
        const bool isBestAskRow = m_snapshot.bestAsk > 0.0 && std::abs(lvl.price - m_snapshot.bestAsk) <= bestPriceTolerance;

        QColor rowColor = m_style.background;
        if (hasBid || hasAsk) {
            rowColor = hasAsk && (!hasBid || lvl.askQty >= lvl.bidQty) ? m_style.ask : m_style.bid;
            if (bookRect.width() > 0) {
                QColor bg = rowColor;
                bg.setAlpha((isBestBidRow || isBestAskRow) ? 150 : 60);
                p.fillRect(bookRect, bg);
            }
        }
        if (rowColor != m_style.background) {
            QColor priceBg = rowColor;
            priceBg.setAlpha((isBestBidRow || isBestAskRow) ? 120 : 40);
            p.fillRect(priceRect, priceBg);
        }

        double dominantQty = 0.0;
        bool volumeIsBid = false;
        if (bidQty > 0.0) {
            dominantQty = bidQty;
            volumeIsBid = true;
        }
        if (askQty > dominantQty) {
            dominantQty = askQty;
            volumeIsBid = false;
        }
        const double notional = dominantQty * std::abs(lvl.price);
        if (notional > 0.0 && bookRect.width() > 8) {
            const QString qtyText = formatQty(notional);
            QFont boldFont = p.font();
            boldFont.setBold(true);
            p.setFont(boldFont);
            QColor qtyColor = volumeIsBid ? m_style.bid : m_style.ask;
            qtyColor.setAlpha(220);
            QRect qtyRect(bookRect.left() + 4, y, bookRect.width() - 6, rowIntHeight);

            int matchedIndex = -1;
            double rangeMin = 0.0;
            double rangeMax = 0.0;
            for (int idx = 0; idx < m_volumeRules.size(); ++idx) {
                if (notional >= m_volumeRules[idx].threshold) {
                    matchedIndex = idx;
                } else {
                    break;
                }
            }
            if (matchedIndex >= 0) {
                const VolumeHighlightRule &matched = m_volumeRules[matchedIndex];
                rangeMin = matched.threshold;
                if (matchedIndex + 1 < m_volumeRules.size()) {
                    rangeMax = m_volumeRules[matchedIndex + 1].threshold;
                } else {
                    rangeMax = rangeMin;
                }

                QColor bg = matched.color.isValid() ? matched.color : QColor("#ffd54f");
                QColor textColor = bg.lightness() < 120 ? QColor("#f0f0f0") : QColor("#1e1e1e");
                bg.setAlpha(220);

                double ratio = 1.0;
                if (rangeMax > rangeMin) {
                    ratio = std::clamp((notional - rangeMin) / (rangeMax - rangeMin), 0.0, 1.0);
                }
                const int totalWidth = bookRect.width();
                int highlightWidth = static_cast<int>(std::round(totalWidth * ratio));
                highlightWidth = std::clamp(highlightWidth, 0, totalWidth);

                if (highlightWidth > 0) {
                    QRect fillRect(bookRect.left(), y, highlightWidth, rowIntHeight);
                    p.fillRect(fillRect, bg);
                }
                p.setPen(textColor);
            } else {
                p.setPen(qtyColor);
            }

            p.drawText(qtyRect, Qt::AlignLeft | Qt::AlignVCenter, qtyText);
            p.setFont(font());
        }

        // Price text with leading-zero compaction.
        const QString text = formatPriceForDisplay(lvl.price, 5);
        p.setPen(m_style.text);
        int textWidth = fm.horizontalAdvance(text);
        int textX = priceRight - textWidth - 2;
        int textY = y + fm.ascent() + (static_cast<int>(rowHeight) - fm.height()) / 2;
        p.drawText(textX, textY, text);

        // Grid line aligned to row top (matches PrintsWidget grid spacing)
        p.setPen(m_style.grid);
        p.drawLine(QPoint(0, y), QPoint(w, y));

        if (i == m_hoverRow) {
            QColor hoverFill(40, 110, 220, 60);
            QRect hoverRect = rowRect;
            hoverRect.setLeft(std::max(0, bookRect.left()));
            hoverRect.setRight(priceRight + 1);
            p.fillRect(hoverRect, hoverFill);
        }
    }

    const bool hasPosition = m_position.hasPosition && m_position.quantity > 0.0 && m_position.averagePrice > 0.0;
    double bestReferencePrice = 0.0;
    if (hasPosition) {
        bestReferencePrice = (m_position.side == OrderSide::Buy) ? m_snapshot.bestBid : m_snapshot.bestAsk;
    }
    if (hasPosition && bestReferencePrice > 0.0) {
        const int rowIdx = rowForPrice(bestReferencePrice);
        if (rowIdx >= 0) {
            const double pnl = (m_position.side == OrderSide::Buy)
                                   ? (bestReferencePrice - m_position.averagePrice) * m_position.quantity
                                   : (m_position.averagePrice - bestReferencePrice) * m_position.quantity;
            const QRect pnlRect(0, rowIdx * m_rowHeight, priceLeft + 1, m_rowHeight);
            QColor pnlColor = pnl >= 0.0 ? QColor("#4caf50") : QColor("#e53935");
            QFont pnlFont = p.font();
            pnlFont.setBold(true);
            p.setFont(pnlFont);
            QString pnlText = QStringLiteral("%1%2%3")
                                  .arg(pnl >= 0.0 ? QStringLiteral("+") : QString())
                                  .arg(QString::number(std::abs(pnl), 'f', std::abs(pnl) >= 1.0 ? 2 : 4))
                                  .arg(QStringLiteral("$"));
            p.setPen(pnlColor);
            p.drawText(pnlRect.adjusted(16, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, pnlText);
            const int arrowSize = 12;
            const int centerY = pnlRect.center().y();
            QPolygon arrow;
            if (pnl >= 0.0) {
                arrow << QPoint(6, centerY + arrowSize / 2)
                      << QPoint(12, centerY + arrowSize / 2)
                      << QPoint(9, centerY - arrowSize / 2);
            } else {
                arrow << QPoint(6, centerY - arrowSize / 2)
                      << QPoint(12, centerY - arrowSize / 2)
                      << QPoint(9, centerY + arrowSize / 2);
            }
            p.setBrush(pnlColor);
            p.setPen(Qt::NoPen);
            p.drawPolygon(arrow);
            p.setPen(m_style.text);
            p.setFont(font());
        }
    }

    const QRect infoRect(0, height() - m_infoAreaHeight, w, m_infoAreaHeight);
    QColor infoBg(0, 0, 0, 180);
    p.fillRect(infoRect, infoBg);
    QString infoText;
    if (hasPosition) {
        double bestPrice = bestReferencePrice;
        double unrealized = 0.0;
        if (bestPrice > 0.0) {
            unrealized = (m_position.side == OrderSide::Buy)
                             ? (bestPrice - m_position.averagePrice) * m_position.quantity
                             : (m_position.averagePrice - bestPrice) * m_position.quantity;
        }
        infoText = tr("Avg %1 | Qty %2 | UPNL %3 | Realized %4")
                       .arg(QString::number(m_position.averagePrice, 'f', 5))
                       .arg(QString::number(m_position.quantity, 'f', 3))
                       .arg(QStringLiteral("%1%2$")
                                .arg(unrealized >= 0.0 ? QStringLiteral("+") : QString())
                                .arg(QString::number(std::abs(unrealized), 'f', std::abs(unrealized) >= 1.0 ? 2 : 4)))
                       .arg(QStringLiteral("%1%2$")
                                .arg(m_position.realizedPnl >= 0.0 ? QStringLiteral("+") : QString())
                                .arg(QString::number(std::abs(m_position.realizedPnl), 'f', std::abs(m_position.realizedPnl) >= 1.0 ? 2 : 4)));
    } else if (!qFuzzyIsNull(m_position.realizedPnl)) {
        infoText = tr("Realized PnL %1%2$")
                       .arg(m_position.realizedPnl >= 0.0 ? QStringLiteral("+") : QString())
                       .arg(QString::number(std::abs(m_position.realizedPnl), 'f', std::abs(m_position.realizedPnl) >= 1.0 ? 2 : 4));
    } else {
        infoText = tr("No active position");
    }
    p.setPen(Qt::white);
    p.drawText(infoRect.adjusted(8, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, infoText);
}

void DomWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_snapshot.levels.isEmpty()) {
        const int rows = m_snapshot.levels.size();
        const int y = event->pos().y();
        const int ladderHeight = rows * m_rowHeight;
        if (y >= 0 && y < ladderHeight) {
            int row = y / m_rowHeight;
            row = std::clamp(row, 0, rows - 1);
            const DomLevel &lvl = m_snapshot.levels[row];
            emit rowClicked(event->button(), row, lvl.price, lvl.bidQty, lvl.askQty);
        }
    }
    QWidget::mousePressEvent(event);
}

QSize DomWidget::sizeHint() const
{
    return QSize(180, 420 + m_infoAreaHeight);
}

QSize DomWidget::minimumSizeHint() const
{
    return QSize(120, 240 + m_infoAreaHeight);
}

void DomWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_snapshot.levels.isEmpty()) {
        const int rows = m_snapshot.levels.size();
        const int y = event->pos().y();
        const int ladderHeight = rows * m_rowHeight;
        int row = -1;
        if (y >= 0 && y < ladderHeight) {
            row = std::clamp(y / m_rowHeight, 0, rows - 1);
        }

        if (row != m_hoverRow) {
            m_hoverRow = row;
            updateHoverInfo(row);
            update();
            if (row >= 0) {
                const DomLevel &lvl = m_snapshot.levels[row];
                emit rowHovered(row, lvl.price, lvl.bidQty, lvl.askQty);
            } else {
                emit rowHovered(-1, 0.0, 0.0, 0.0);
            }
        }
    }

    QWidget::mouseMoveEvent(event);
}

void DomWidget::leaveEvent(QEvent *event)
{
    if (m_hoverRow != -1) {
        m_hoverRow = -1;
        updateHoverInfo(-1);
        update();
        emit rowHovered(-1, 0.0, 0.0, 0.0);
    }
    QWidget::leaveEvent(event);
}

void DomWidget::updateHoverInfo(int row)
{
    if (row < 0 || row >= m_snapshot.levels.size()) {
        m_hoverInfoText.clear();
        emit hoverInfoChanged(-1, 0.0, QString());
        return;
    }

    const DomLevel &lvl = m_snapshot.levels[row];
    const double notional = std::max(lvl.bidQty, lvl.askQty) * std::abs(lvl.price);
    const double cumulative = cumulativeNotionalForRow(row);
    double pct = 0.0;
    QString percentText;
    if (percentFromReference(lvl.price, m_snapshot.bestBid, m_snapshot.bestAsk, pct)) {
        percentText = QString::number(pct, 'f', std::abs(pct) >= 0.1 ? 2 : 3) + QLatin1String("%");
    } else {
        percentText = QStringLiteral("-");
    }

    QStringList parts;
    parts << percentText;
    if (cumulative > 0.0) {
        parts << formatValueShort(cumulative);
    } else if (notional > 0.0) {
        parts << formatValueShort(notional);
    }
    m_hoverInfoText = parts.join(QStringLiteral(" | "));
    emit hoverInfoChanged(row, lvl.price, m_hoverInfoText);
}

double DomWidget::cumulativeNotionalForRow(int row) const
{
    if (row < 0 || row >= m_snapshot.levels.size()) {
        return 0.0;
    }
    if (m_snapshot.bestBid <= 0.0 && m_snapshot.bestAsk <= 0.0) {
        return 0.0;
    }

    const DomLevel &target = m_snapshot.levels[row];
    const double tol = priceTolerance(m_snapshot.tickSize);
    const double targetPrice = target.price;
    double total = 0.0;

    if (m_snapshot.bestBid > 0.0 && targetPrice <= m_snapshot.bestBid + tol) {
        const double lower = std::min(targetPrice, m_snapshot.bestBid);
        const double upper = std::max(targetPrice, m_snapshot.bestBid);
        for (const DomLevel &lvl : m_snapshot.levels) {
            if (lvl.bidQty <= 0.0) {
                continue;
            }
            if (lvl.price >= lower - tol && lvl.price <= upper + tol) {
                total += lvl.bidQty * std::abs(lvl.price);
            }
        }
        return total;
    }

    if (m_snapshot.bestAsk > 0.0 && targetPrice >= m_snapshot.bestAsk - tol) {
        const double lower = std::min(targetPrice, m_snapshot.bestAsk);
        const double upper = std::max(targetPrice, m_snapshot.bestAsk);
        for (const DomLevel &lvl : m_snapshot.levels) {
            if (lvl.askQty <= 0.0) {
                continue;
            }
            if (lvl.price >= lower - tol && lvl.price <= upper + tol) {
                total += lvl.askQty * std::abs(lvl.price);
            }
        }
        return total;
    }

    return 0.0;
}

void DomWidget::setVolumeHighlightRules(const QVector<VolumeHighlightRule> &rules)
{
    m_volumeRules = rules;
    std::sort(m_volumeRules.begin(), m_volumeRules.end(), [](const VolumeHighlightRule &a, const VolumeHighlightRule &b) {
        return a.threshold < b.threshold;
    });
    update();
}

void DomWidget::setTradePosition(const TradePosition &position)
{
    m_position = position;
    update();
}

void DomWidget::setLocalOrders(const QVector<LocalOrderMarker> &orders)
{
    m_localOrders = orders;
    update();
}

int DomWidget::rowForPrice(double price) const
{
    if (m_snapshot.levels.isEmpty()) {
        return -1;
    }
    int closest = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (int i = 0; i < m_snapshot.levels.size(); ++i) {
        const double dist = std::abs(m_snapshot.levels[i].price - price);
        if (dist < bestDist) {
            bestDist = dist;
            closest = i;
        }
    }
    return closest;
}

#include "PrintsWidget.h"

#include <QFontMetrics>
#include <QFontInfo>
#include <QPainter>
#include <QPaintEvent>
#include <QTimerEvent>
#include <QtGlobal>
#include <QDebug>
#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
QString formatQty(double v)
{
    const double av = std::abs(v);
    if (av >= 1000000.0) return QString::number(av / 1000000.0, 'f', 1) + "M";
    if (av >= 1000.0) return QString::number(av / 1000.0, 'f', 1) + "K";
    return QString::number(av, 'f', av >= 10.0 ? 0 : 1);
}
} // namespace

PrintsWidget::PrintsWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PrintsWidget::setPrints(const QVector<PrintItem> &items)
{
    // Keep rowHint as provided by LadderClient; also use it to calibrate offset vs price rows.
    m_items = items;
    for (const auto &it : m_items) {
        if (it.rowHint >= 0) {
            const int priceRow = rowForPrice(it.price);
            if (priceRow >= 0) {
                calibrateRowOffset(it.rowHint, priceRow);
            }
        }
    }

    QHash<QString, double> nextProgress;
    nextProgress.reserve(items.size());
    bool hasNew = false;
    for (const auto &it : m_items) {
        const QString key = makeKey(it);
        if (m_spawnProgress.contains(key)) {
            nextProgress.insert(key, m_spawnProgress.value(key));
        } else {
            nextProgress.insert(key, 0.0);
            hasNew = true;
        }
    }
    m_spawnProgress = nextProgress;
    bool needTimer = false;
    for (auto it = m_spawnProgress.cbegin(); it != m_spawnProgress.cend(); ++it) {
        if (it.value() < 0.999) {
            needTimer = true;
            break;
        }
    }
    if (needTimer) {
        if (!m_animTimer.isActive()) {
            m_animTimer.start(16, this);
        }
    } else {
        m_animTimer.stop();
    }
    update();
}

void PrintsWidget::setLadderPrices(const QVector<double> &prices, int rowHeight, double tickSize)
{
    const bool resetMapping = m_prices.isEmpty() || prices.isEmpty();
    m_prices = prices;
    m_priceToRow.clear();
    m_priceToRow.reserve(m_prices.size());
    for (int i = 0; i < m_prices.size(); ++i) {
        m_priceToRow.insert(m_prices[i], i);
    }
    m_firstPrice = m_prices.isEmpty() ? 0.0 : m_prices.first();
    m_descending = m_prices.size() < 2 ? true : (m_prices.first() > m_prices.last());
    if (tickSize > 0.0) {
        m_tickSize = tickSize;
    } else {
        m_tickSize = 0.0;
        for (int i = 1; i < m_prices.size(); ++i) {
            const double diff = std::abs(m_prices[i - 1] - m_prices[i]);
            if (diff > 1e-9) {
                m_tickSize = diff;
                break;
            }
        }
    }
    m_rowHeight = std::max(10, std::min(rowHeight, 40));

    const int totalHeight = m_prices.size() * m_rowHeight + kDomInfoAreaHeight;
    if (resetMapping) {
        m_rowOffset = 0;
        m_rowOffsetValid = false;
    }
    if (m_hoverRow >= m_prices.size()) {
        m_hoverRow = -1;
        m_hoverText.clear();
    }
    setMinimumHeight(totalHeight);
    setMaximumHeight(totalHeight);
    updateGeometry();
    update();
}

void PrintsWidget::setRowHeightOnly(int rowHeight)
{
    m_rowHeight = std::max(10, std::min(rowHeight, 40));
    const int totalHeight = m_prices.size() * m_rowHeight + kDomInfoAreaHeight;
    setMinimumHeight(totalHeight);
    setMaximumHeight(totalHeight);
    updateGeometry();
    update();
}

void PrintsWidget::setLocalOrders(const QVector<LocalOrderMarker> &orders)
{
    m_orderMarkers = orders;
    update();
}

void PrintsWidget::setHoverInfo(int row, double price, const QString &text)
{
    const int rowCount = m_prices.size();
    const bool domRowValid = (row >= 0 && row < rowCount);
    int resolvedRow = -1;
    const bool priceValid = (rowCount > 0) && std::isfinite(price);
    int priceRow = -1;
    if (priceValid) {
        priceRow = rowForPrice(price);
    }
    if (domRowValid && priceValid && priceRow >= 0) {
        calibrateRowOffset(row, priceRow);
    }
    resolvedRow = domRowValid ? row : applyRowOffset(priceRow);
    QString newText = resolvedRow >= 0 ? text : QString();
    if (m_hoverRow == resolvedRow && m_hoverText == newText && m_hoverPriceValid == priceValid) {
        if (!priceValid || qFuzzyCompare(m_hoverPrice, price)) {
            return;
        }
    }
    m_hoverRow = resolvedRow;
    m_hoverText = newText;
    m_hoverPriceValid = priceValid;
    m_hoverPrice = priceValid ? price : 0.0;
    update();
}

void PrintsWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(rect(), QColor("#151515"));

    if (m_prices.isEmpty()) {
        return;
    }

    QFont boldFont = font();
    boldFont.setBold(true);
    p.setFont(boldFont);
    QFontMetrics fm(boldFont);
    const int basePixelSize = std::max(6, QFontInfo(boldFont).pixelSize() > 0 ? QFontInfo(boldFont).pixelSize()
                                                                              : fm.height());
    const int w = width();

    // Horizontal grid lines to match ladder rows.
    p.setPen(QColor("#303030"));
    const int rows = m_prices.size();
    for (int i = 0; i <= rows; ++i) {
        const int y = i * m_rowHeight;
        p.drawLine(0, y, w, y);
    }

    bool hasHoverRow = m_hoverRow >= 0 && m_hoverRow < rows;
    QRect hoverRowRect;
    if (hasHoverRow) {
        // Use exact rowHeight for the hover bar to avoid bleeding into the next row.
        hoverRowRect = QRect(0, m_hoverRow * m_rowHeight, w, m_rowHeight);
    }

    const int count = m_items.size();

    // Map a print to the y coordinate: prefer its rowHint, else fallback to nearest price.
    auto itemToY = [&](const PrintItem &item, int *outRowIdx = nullptr) -> int {
        int rowIdx = item.rowHint;
        if (rowIdx < 0 || rowIdx >= m_prices.size()) {
            rowIdx = rowForPrice(item.price);
        }
        if (rowIdx >= 0) {
            rowIdx = applyRowOffset(rowIdx);
        }
        if (outRowIdx) {
            *outRowIdx = rowIdx;
        }
        if (rowIdx < 0) {
            return m_rowHeight / 2;
        }
        // Slight visual lift: anchor near the top of the row (1px above center).
        return rowIdx * m_rowHeight + (m_rowHeight / 2) - 1;
    };

    if (count > 0) {
    // Lay prints left-to-right in time: older on the left, newest on the right.
    const int padding = 6;
    const int slotCount = std::max(6, w / 24);
    const double slotW = static_cast<double>(w - padding * 2) /
                         static_cast<double>(slotCount > 0 ? slotCount : 1);
    const int startIdx = count > slotCount ? (count - slotCount) : 0;

    // Draw connecting lines between sequential prints (visible window only).
    p.setPen(QPen(QColor("#444444"), 1));
    for (int i = startIdx + 1; i < count; ++i) {
        int rowIdx1 = -1;
        int rowIdx2 = -1;
        const int y1 = itemToY(m_items[i - 1], &rowIdx1);
        const int y2 = itemToY(m_items[i], &rowIdx2);
        const double x1 = padding + (i - 1 - startIdx) * slotW + slotW * 0.5;
        const double x2 = padding + (i - startIdx) * slotW + slotW * 0.5;
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    for (int i = startIdx; i < count; ++i) {
        const PrintItem &it = m_items[i];
        int rowIdx = -1;
        const int y = itemToY(it, &rowIdx);
        const double xCenter = padding + (i - startIdx) * slotW + slotW * 0.5;

        const double magnitude = std::log10(1.0 + std::abs(it.qty));
        const QString key = makeKey(it);
        double spawn = m_spawnProgress.value(key, 1.0);
        spawn = std::clamp(spawn, 0.0, 1.0);
        const double eased = 1.0 - std::pow(1.0 - spawn, 3.0);

        const int baseRadius = std::max(10, std::min(18, 9 + static_cast<int>(std::round(magnitude * 5.0))));
        const int animatedRadius = static_cast<int>(baseRadius * (0.8 + 0.2 * eased));
        QRect circleRect(static_cast<int>(xCenter - animatedRadius), y - animatedRadius, animatedRadius * 2, animatedRadius * 2);
        QColor c = it.buy ? QColor("#4caf50") : QColor("#e53935");
        c.setAlpha(static_cast<int>(210 * (0.7 + 0.3 * eased)));
        p.setBrush(c);
        QColor border = it.buy ? QColor("#2f6c37") : QColor("#992626");
        p.setPen(QPen(border, 2));
        p.drawEllipse(circleRect);

        // label
        p.setPen(Qt::white);
        const QString text = formatQty(it.qty);
        QFont textFont = boldFont;
        QFontMetrics textFm(textFont);
        int pixelSize = basePixelSize;
        const double available = std::max(4, circleRect.width() - 4);
        double textExtent = std::max(textFm.horizontalAdvance(text), textFm.height());
        if (pixelSize > 0 && textExtent > available) {
            const double scale = std::clamp(available / textExtent, 0.5, 1.0);
            const int newPixel = std::max(6, static_cast<int>(std::floor(pixelSize * scale)));
            textFont.setPixelSize(newPixel);
            textFm = QFontMetrics(textFont);
        }
        p.setFont(textFont);
        p.drawText(circleRect, Qt::AlignCenter, text);
        p.setFont(boldFont);
    }

    }

        // Local order markers hugging правый край (к DOM), агрегируя объём на тик.
    if (!m_orderMarkers.isEmpty()) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        QFont markerFont = font();
        markerFont.setBold(true);
        QFontMetrics fmMarker(markerFont);

        struct Agg {
            double qty = 0.0;
            qint64 createdMs = 0;
            bool buy = true;
            int row = -1;
        };
        QHash<quint64, Agg> agg;
        for (const auto &ord : m_orderMarkers) {
            int rowIdx = rowForPrice(ord.price);
            if (rowIdx >= 0) {
                rowIdx = applyRowOffset(rowIdx);
            }
            if (rowIdx < 0 || rowIdx >= rows) {
                continue;
            }
            const quint64 key = (static_cast<quint64>(rowIdx) << 1) | (ord.buy ? 1ULL : 0ULL);
            Agg a = agg.value(key, Agg{});
            a.qty += std::max(0.0, ord.quantity);
            a.buy = ord.buy;
            a.row = rowIdx;
            a.createdMs = (a.createdMs == 0) ? ord.createdMs : std::min(a.createdMs, ord.createdMs);
            agg.insert(key, a);
        }

        for (const auto &a : agg) {
            const QString text = formatQty(a.qty);
            const int markerHeight = std::clamp(m_rowHeight - 2, 14, 28);
            const int tip = std::clamp(markerHeight / 2, 8, 14);
            const int textWidth = fmMarker.horizontalAdvance(text);
            const int baseWidth = textWidth + tip + 10;
            const int markerWidth = std::min(std::max(baseWidth, tip * 2 + 14),
                                             std::max(60, w - 12));

            const int yCenter = a.row * m_rowHeight + (m_rowHeight / 2);
            const int top = yCenter - markerHeight / 2;
            const int bottom = top + markerHeight;
            const int right = w - 2;
            const int left = right - markerWidth;
            const int midY = (top + bottom) / 2;

            qint64 age = nowMs - a.createdMs;
            const qint64 fadeWindow = 20000;
            double fade = 1.0;
            if (age > fadeWindow) {
                fade = 0.35;
            } else if (age > 0) {
                fade = 1.0 - (static_cast<double>(age) / fadeWindow) * 0.65;
            }
            QColor base = a.buy ? QColor("#4caf50") : QColor("#ef5350");
            int alpha = std::clamp(static_cast<int>(180 * fade), 50, 210);
            base.setAlpha(alpha);
            QColor edge = a.buy ? QColor("#2f6c37") : QColor("#992626");
            edge.setAlpha(std::clamp(alpha + 30, 80, 230));

            const int tipOffset = tip;
            QPolygon env;
            env << QPoint(left, top)
                << QPoint(right - tipOffset, top)
                << QPoint(right, midY)
                << QPoint(right - tipOffset, bottom)
                << QPoint(left, bottom);
            p.setBrush(base);
            p.setPen(Qt::NoPen);
            p.drawPolygon(env);
            p.setPen(QPen(edge, 1.4));
            p.drawPolygon(env);

            p.setFont(markerFont);
            QColor tcol = QColor("#f7f9fb");
            tcol.setAlpha(245);
            p.setPen(tcol);
            QRect textRect(left + 2, top, markerWidth - tipOffset - 4, markerHeight);
            p.drawText(textRect, Qt::AlignCenter, text);
        }
        p.setFont(font());
    }
    if (hasHoverRow && !m_hoverText.isEmpty()) {
        QFont infoFont = font();
        infoFont.setBold(false);
        if (infoFont.pointSizeF() > 0.0) {
            infoFont.setPointSizeF(std::max(6.0, infoFont.pointSizeF() - 1.0));
        } else {
            infoFont.setPointSize(std::max(6, infoFont.pointSize() - 1));
        }
        QFontMetrics infoFm(infoFont);
        const int padding = 6;
        const int desiredWidth = infoFm.horizontalAdvance(m_hoverText) + padding * 2;
        const int barWidth = std::min(hoverRowRect.width(), desiredWidth);
        QRect highlightRect(hoverRowRect.right() - barWidth, hoverRowRect.top(), barWidth, hoverRowRect.height());
        QColor hoverFill(40, 110, 220, 60);
        p.fillRect(highlightRect, hoverFill);
        p.setFont(infoFont);
        p.setPen(Qt::white);
        QRect textRect = highlightRect.adjusted(padding / 2, 0, -padding / 2, 0);
        p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, m_hoverText);
        p.setFont(boldFont);
    } else if (hasHoverRow) {
        QColor hoverFill(40, 110, 220, 60);
        p.fillRect(hoverRowRect, hoverFill);
    }
}

void PrintsWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_animTimer.timerId()) {
        bool any = false;
        for (auto it = m_spawnProgress.begin(); it != m_spawnProgress.end(); ++it) {
            double value = it.value();
            if (value < 0.999) {
                value += (1.0 - value) * 0.3;
                if (value > 0.999) {
                    value = 1.0;
                } else {
                    any = true;
                }
                it.value() = value;
            }
        }
        if (!any) {
            m_animTimer.stop();
        }
        update();
        return;
    }
    QWidget::timerEvent(event);
}

QString PrintsWidget::makeKey(const PrintItem &item) const
{
    return QStringLiteral("%1_%2_%3")
        .arg(item.price, 0, 'f', 5)
        .arg(item.qty, 0, 'f', 3)
        .arg(item.buy ? QLatin1Char('B') : QLatin1Char('S'));
}

int PrintsWidget::rowForPrice(double price) const
{
    if (m_prices.isEmpty()) {
        return -1;
    }
    // First try exact match; DOM and prints share the same ladder values.
    auto it = m_priceToRow.constFind(price);
    if (it != m_priceToRow.constEnd()) {
        return it.value();
    }

    // If ladder is sorted descending (high -> low), use tickSize to snap index.
    if (m_tickSize > 0.0 && m_prices.size() > 1) {
        const bool descending = m_prices.first() > m_prices.last();
        double delta = descending ? (m_prices.first() - price) / m_tickSize
                                  : (price - m_prices.first()) / m_tickSize;
        const int idx = static_cast<int>(std::llround(delta));
        if (idx >= 0 && idx < m_prices.size()) {
            return idx;
        }
    }

    int bestIdx = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (int i = 0; i < m_prices.size(); ++i) {
        const double d = std::abs(m_prices[i] - price);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int PrintsWidget::applyRowOffset(int row) const
{
    if (row < 0 || m_prices.isEmpty()) {
        return row;
    }
    const int maxRow = static_cast<int>(m_prices.size()) - 1;
    int adjusted = row;
    if (m_rowOffsetValid) {
        adjusted += m_rowOffset;
    }
    adjusted = std::clamp<int>(adjusted, 0, maxRow);
    return adjusted;
}

void PrintsWidget::calibrateRowOffset(int domRow, int priceRow)
{
    if (domRow < 0 || priceRow < 0) {
        return;
    }
    const int diff = domRow - priceRow;
    const int maxOffset = std::max<int>(4, static_cast<int>(m_prices.size()) / 5);
    if (std::abs(diff) > maxOffset) {
        return;
    }
    if (!m_rowOffsetValid || diff != m_rowOffset) {
        m_rowOffset = diff;
        m_rowOffsetValid = true;
    }
}

QSize PrintsWidget::sizeHint() const
{
    return QSize(120, 400);
}

QSize PrintsWidget::minimumSizeHint() const
{
    return QSize(80, 200);
}

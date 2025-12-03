#include "LadderClient.h"
#include "PrintsWidget.h"

#include <QDateTime>
#include <QDebug>

#include <json.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>

using json = nlohmann::json;

LadderClient::LadderClient(const QString &backendPath,
                           const QString &symbol,
                           int levels,
                          DomWidget *dom,
                          QObject *parent,
                          PrintsWidget *prints)
    : QObject(parent)
    , m_backendPath(backendPath)
    , m_symbol(symbol)
    , m_levels(levels)
    , m_dom(dom)
    , m_initialCenterSent(false)
    , m_prints(prints)
{
    m_process.setProgram(m_backendPath);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    connect(&m_process, &QProcess::readyReadStandardOutput, this, &LadderClient::handleReadyRead);
    connect(&m_process,
            &QProcess::errorOccurred,
            this,
            &LadderClient::handleErrorOccurred);
    connect(&m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &LadderClient::handleFinished);

    m_watchdogTimer.setSingleShot(true);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &LadderClient::handleWatchdogTimeout);

    restart(m_symbol, m_levels);
}

void LadderClient::restart(const QString &symbol, int levels)
{
    m_symbol = symbol;
    m_levels = levels;
    m_initialCenterSent = false;
    m_lastTickSize = 0.0;
    m_printBuffer.clear();
    if (m_prints) {
        QVector<PrintItem> emptyPrints;
        m_prints->setPrints(emptyPrints);
        QVector<double> emptyPrices;
        const int rowH = m_dom ? m_dom->rowHeight() : 20;
        m_prints->setLadderPrices(emptyPrices, rowH, 0.0);
        QVector<LocalOrderMarker> emptyOrders;
        m_prints->setLocalOrders(emptyOrders);
    }

    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(2000);
    }
    m_lastPrices.clear();

    QStringList args;
    args << "--symbol" << m_symbol << "--ladder-levels" << QString::number(m_levels);
    m_process.setArguments(args);

    emitStatus(QStringLiteral("Starting backend (%1, %2 levels)...").arg(m_symbol).arg(m_levels));
    m_process.start();
    armWatchdog();
}

void LadderClient::stop()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(2000);
        emitStatus(QStringLiteral("Backend stopped"));
    }
    m_watchdogTimer.stop();
}

bool LadderClient::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void LadderClient::setCompression(int factor)
{
    m_tickCompression = std::max(1, factor);
}

void LadderClient::handleReadyRead()
{
    emitStatus("Receiving data...");
    m_buffer += m_process.readAllStandardOutput();
    int idx = -1;
    while ((idx = m_buffer.indexOf('\n')) != -1) {
        QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        if (!line.trimmed().isEmpty()) {
            processLine(line);
        }
    }
}

void LadderClient::handleErrorOccurred(QProcess::ProcessError error)
{
    qWarning() << "[LadderClient] backend error" << error << m_process.errorString();
    emitStatus("Backend error: " + m_process.errorString());
}

void LadderClient::handleFinished(int exitCode, QProcess::ExitStatus status)
{
    qWarning() << "[LadderClient] backend finished" << exitCode << status;
    emitStatus(QString("Backend finished (%1)").arg(exitCode));
    m_watchdogTimer.stop();
}

void LadderClient::processLine(const QByteArray &line)
{
    json j;
    try {
        j = json::parse(line.constData(), line.constData() + line.size());
    } catch (const std::exception &ex) {
        qWarning() << "[LadderClient] parse error:" << ex.what();
        emitStatus("Parse error: " + QString::fromUtf8(ex.what()));
        return;
    }

    const std::string type = j.value("type", std::string());
    armWatchdog();
    if (type == "trade") {
        if (!m_prints) {
            return;
        }
        double price = j.value("price", 0.0);
        const double qtyBase = j.value("qty", 0.0);
        const std::string side = j.value("side", std::string("buy"));
        if (price <= 0.0 || qtyBase <= 0.0) {
            return;
        }
        if (m_lastPrices.isEmpty()) {
            // Don't render until we have ladder prices to align to
            return;
        }
        if (m_lastTickSize > 0.0) {
            // Snap trade price to the current tick grid so it aligns with ladder rows.
            const auto tick = static_cast<std::int64_t>(std::llround(price / m_lastTickSize));
            price = static_cast<double>(tick) * m_lastTickSize;
        }

        int bestIdx = -1;
        if (!m_lastPrices.isEmpty()) {
            bestIdx = 0;
            double bestDiff = std::numeric_limits<double>::max();
            for (int i = 0; i < m_lastPrices.size(); ++i) {
                const double diff = std::abs(m_lastPrices[i] - price);
                if (diff < bestDiff) {
                    bestDiff = diff;
                    bestIdx = i;
                }
            }
            price = m_lastPrices.value(bestIdx, price);
        }

        const double qtyQuote = price * qtyBase;
        if (qtyQuote <= 0.0) {
            return;
        }

        PrintItem it;
        it.price = price;
        // Show quote (USDT) notional in the UI circles.
        it.qty = qtyQuote;
        it.buy = (side != "sell");
        it.rowHint = bestIdx;
        m_printBuffer.push_back(it);
        const int maxPrints = 200;
        if (m_printBuffer.size() > maxPrints) {
            m_printBuffer.erase(m_printBuffer.begin(),
                                m_printBuffer.begin() + (m_printBuffer.size() - maxPrints));
        }
        // ladderPrices уже приходят из ladder-сообщений; здесь только добавляем принт
        m_prints->setPrints(m_printBuffer);
        return;
    }

    if (type != "ladder") {
        return;
    }

    DomSnapshot snap;
    snap.bestBid = j.value("bestBid", 0.0);
    snap.bestAsk = j.value("bestAsk", 0.0);
    snap.tickSize = j.value("tickSize", 0.0);
    if (snap.tickSize > 0.0) {
        m_lastTickSize = snap.tickSize;
    }

    auto rowsIt = j.find("rows");
    if (rowsIt != j.end() && rowsIt->is_array()) {
        for (const auto &row : *rowsIt) {
            DomLevel lvl;
            lvl.price = row.value("price", 0.0);
            lvl.bidQty = row.value("bid", 0.0);
            lvl.askQty = row.value("ask", 0.0);
            snap.levels.push_back(lvl);
        }
        // Ensure levels are sorted top-to-bottom by price so DOM/prints share the same order
        // regardless of backend row order.
        std::sort(snap.levels.begin(), snap.levels.end(), [](const DomLevel &a, const DomLevel &b) {
            return a.price > b.price;
        });

        // Compression: агрегируем уровни в корзины по m_tickCompression тиков.
        if (m_tickCompression > 1 && snap.tickSize > 0.0) {
            std::map<std::int64_t, DomLevel, std::greater<std::int64_t>> buckets;
            for (const auto &lvl : snap.levels) {
                const auto tick = static_cast<std::int64_t>(std::llround(lvl.price / snap.tickSize));
                const std::int64_t bucketTick = (tick / m_tickCompression) * m_tickCompression;
                DomLevel &dst = buckets[bucketTick];
                dst.price = static_cast<double>(bucketTick) * snap.tickSize;
                dst.bidQty += lvl.bidQty;
                dst.askQty += lvl.askQty;
            }
            snap.levels.clear();
            snap.levels.reserve(static_cast<int>(buckets.size()));
            for (const auto &kv : buckets) {
                snap.levels.push_back(kv.second);
            }

            // Пересчитать bestBid/bestAsk на ближайшее значение из агрегированных корзин,
            // чтобы подсветка соответствовала фактическому биду/аску внутри бинов.
            auto snapToBucket = [](double ref, const QVector<DomLevel> &levels) -> double {
                if (ref <= 0.0 || levels.isEmpty()) {
                    return ref;
                }
                double best = levels.first().price;
                double bestDist = std::abs(levels.first().price - ref);
                for (const auto &lvl : levels) {
                    const double d = std::abs(lvl.price - ref);
                    if (d < bestDist) {
                        bestDist = d;
                        best = lvl.price;
                    }
                }
                return best;
            };
            snap.bestBid = snapToBucket(snap.bestBid, snap.levels);
            snap.bestAsk = snapToBucket(snap.bestAsk, snap.levels);
        }
    }

    // Ping calculation from backend timestamp, if available.
    const auto tsIt = j.find("timestamp");
    if (tsIt != j.end() && tsIt->is_number_integer()) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 tsMs = static_cast<qint64>(tsIt->get<std::int64_t>());
        const int pingMs = static_cast<int>(std::max<qint64>(0, nowMs - tsMs));
        emit pingUpdated(pingMs);
        emitStatus(QStringLiteral("ping %1 ms").arg(pingMs));
    } else {
        emitStatus(QStringLiteral("Snapshot received"));
    }

    if (m_dom) {
        double centerPrice = 0.0;
        if (snap.bestBid > 0.0 && snap.bestAsk > 0.0) {
            centerPrice = (snap.bestBid + snap.bestAsk) * 0.5;
        } else if (snap.bestBid > 0.0) {
            centerPrice = snap.bestBid;
        } else if (snap.bestAsk > 0.0) {
            centerPrice = snap.bestAsk;
        }
        if (centerPrice > 0.0 && !m_initialCenterSent) {
            m_dom->setInitialCenterPrice(centerPrice);
            m_initialCenterSent = true;
        }
        m_dom->updateSnapshot(snap);
    }

    if (m_prints) {
        m_lastPrices.clear();
        m_lastPrices.reserve(snap.levels.size());
        for (const auto &lvl : snap.levels) {
            m_lastPrices.push_back(lvl.price);
        }
        const int rowH = m_dom ? m_dom->rowHeight() : 20;
        const double tickForPrints = snap.tickSize > 0.0 ? snap.tickSize : m_lastTickSize;
        m_prints->setLadderPrices(m_lastPrices, rowH, tickForPrints * m_tickCompression);
    }
}

void LadderClient::emitStatus(const QString &msg)
{
    emit statusMessage(msg);
}

void LadderClient::armWatchdog()
{
    m_lastUpdateMs = QDateTime::currentMSecsSinceEpoch();
    if (m_watchdogIntervalMs > 0) {
        m_watchdogTimer.start(m_watchdogIntervalMs);
    }
}

void LadderClient::handleWatchdogTimeout()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastUpdateMs < m_watchdogIntervalMs - 50) {
        // Data arrived while timer was firing.
        return;
    }
    emitStatus(QStringLiteral("No data received for %1s, restarting backend...")
                   .arg(m_watchdogIntervalMs / 1000));
    restart(m_symbol, m_levels);
}

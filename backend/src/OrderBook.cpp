#include "OrderBook.hpp"

#include <algorithm>
#include <limits>

namespace dom
{
    OrderBook::OrderBook() = default;

    void OrderBook::clear()
    {
        bids_.clear();
        asks_.clear();
        // tickSize_ is configured separately via setTickSize()
        centerTick_ = 0;
        hasCenter_ = false;
    }

    void OrderBook::setTickSize(double tickSize)
    {
        tickSize_ = tickSize > 0.0 ? tickSize : 0.0;
    }

    void OrderBook::loadSnapshot(const std::vector<std::pair<Tick, double>>& bids,
                                 const std::vector<std::pair<Tick, double>>& asks)
    {
        clear();

        for (const auto& [tick, qty] : bids)
        {
            if (qty > 0.0)
            {
                bids_[tick] += qty;
            }
        }

        for (const auto& [tick, qty] : asks)
        {
            if (qty > 0.0)
            {
                asks_[tick] += qty;
            }
        }
    }

    void OrderBook::applyDelta(const std::vector<std::pair<Tick, double>>& bids,
                               const std::vector<std::pair<Tick, double>>& asks)
    {
        applySide(bids_, bids);
        applySide(asks_, asks);
    }

    double OrderBook::bestBid() const
    {
        if (bids_.empty() || tickSize_ <= 0.0)
        {
            return 0.0;
        }
        const Tick tick = bids_.rbegin()->first;
        return static_cast<double>(tick) * tickSize_;
    }

    double OrderBook::bestAsk() const
    {
        if (asks_.empty() || tickSize_ <= 0.0)
        {
            return 0.0;
        }
        const Tick tick = asks_.begin()->first;
        return static_cast<double>(tick) * tickSize_;
    }

    double OrderBook::tickSize() const
    {
        return tickSize_;
    }

    std::vector<Level> OrderBook::ladder(std::size_t levelsPerSide) const
    {
        std::vector<Level> result;

        if (tickSize_ <= 0.0)
        {
            return result;
        }

        if (bids_.empty() && asks_.empty())
        {
            return result;
        }

        // Center around best bid / best ask with some inertia
        // so that the ladder does not jump every tick.
        Tick midTick = 0;
        bool hasMid = false;

        if (!bids_.empty() && !asks_.empty())
        {
            const Tick bestBidTick = bids_.rbegin()->first;
            const Tick bestAskTick = asks_.begin()->first;
            midTick = (bestBidTick + bestAskTick) / 2;
            hasMid = true;
        }
        else if (!bids_.empty())
        {
            midTick = bids_.rbegin()->first;
            hasMid = true;
        }
        else if (!asks_.empty())
        {
            midTick = asks_.begin()->first;
            hasMid = true;
        }

        if (!hasMid)
        {
            return result;
        }

        constexpr Tick maxLevels = 4000;

        // Special mode: levelsPerSide == 0 means "full current book"
        // (bounded only by maxLevels). We don't keep a sliding window here,
        // we just cover from min(bids/asks) to max(bids/asks).
        if (levelsPerSide == 0)
        {
            Tick minTick = std::numeric_limits<Tick>::max();
            Tick maxTick = std::numeric_limits<Tick>::min();

            if (!bids_.empty())
            {
                minTick = std::min(minTick, bids_.begin()->first);
                maxTick = std::max(maxTick, bids_.rbegin()->first);
            }
            if (!asks_.empty())
            {
                minTick = std::min(minTick, asks_.begin()->first);
                maxTick = std::max(maxTick, asks_.rbegin()->first);
            }

            if (minTick > maxTick)
            {
                return result;
            }

            Tick count = maxTick - minTick + 1;
            if (count > maxLevels)
            {
                minTick = maxTick - (maxLevels - 1);
                count = maxLevels;
            }

            result.reserve(static_cast<std::size_t>(count));
            for (Tick tick = maxTick; tick >= minTick; --tick)
            {
                const double price = static_cast<double>(tick) * tickSize_;

                double bidQty = 0.0;
                double askQty = 0.0;

                auto bidIt = bids_.find(tick);
                if (bidIt != bids_.end())
                {
                    bidQty = bidIt->second;
                }

                auto askIt = asks_.find(tick);
                if (askIt != asks_.end())
                {
                    askQty = askIt->second;
                }

                result.push_back(Level{price, bidQty, askQty});

                if (tick == std::numeric_limits<Tick>::min())
                {
                    break;
                }
            }

            return result;
        }

        const Tick padding = static_cast<Tick>(levelsPerSide);
        if (!hasCenter_)
        {
            centerTick_ = midTick;
            hasCenter_ = true;
        }
        else if (padding > 0)
        {
            // Current window around stored center.
            Tick windowMin = centerTick_ - padding;
            Tick windowMax = centerTick_ + padding;

            // Use an inner band; as long as mid stays inside,
            // we do not move the center. This gives a stable ladder.
            const Tick margin = padding / 4;
            const Tick innerMin = windowMin + margin;
            const Tick innerMax = windowMax - margin;

            if (midTick < innerMin)
            {
                // Shift center down so that midTick is closer to middle again.
                centerTick_ = midTick + (padding - margin);
            }
            else if (midTick > innerMax)
            {
                centerTick_ = midTick - (padding - margin);
            }
        }

        Tick maxTick;
        if (centerTick_ > std::numeric_limits<Tick>::max() - padding)
        {
            maxTick = std::numeric_limits<Tick>::max();
        }
        else
        {
            maxTick = centerTick_ + padding;
        }

        Tick minTick;
        if (centerTick_ < std::numeric_limits<Tick>::min() + padding)
        {
            minTick = std::numeric_limits<Tick>::min();
        }
        else
        {
            minTick = centerTick_ - padding;
        }

        if (maxTick < minTick)
        {
            return result;
        }

        Tick count = maxTick - minTick + 1;
        if (count > maxLevels)
        {
            minTick = maxTick - (maxLevels - 1);
            count = maxLevels;
        }

        result.reserve(static_cast<std::size_t>(count));

        for (Tick tick = maxTick; tick >= minTick; --tick)
        {
            const double price = static_cast<double>(tick) * tickSize_;

            double bidQty = 0.0;
            double askQty = 0.0;

            auto bidIt = bids_.find(tick);
            if (bidIt != bids_.end())
            {
                bidQty = bidIt->second;
            }

            auto askIt = asks_.find(tick);
            if (askIt != asks_.end())
            {
                askQty = askIt->second;
            }

            result.push_back(Level{price, bidQty, askQty});

            if (tick == std::numeric_limits<Tick>::min())
            {
                break; // prevent overflow on next --tick
            }
        }

        return result;
    }

    void OrderBook::applySide(BookSide& side, const std::vector<std::pair<Tick, double>>& updates)
    {
        for (const auto& [tick, qty] : updates)
        {
            if (qty <= 0.0)
            {
                auto it = side.find(tick);
                if (it != side.end())
                {
                    side.erase(it);
                }
            }
            else
            {
                side[tick] = qty;
            }
        }
    }
} // namespace dom

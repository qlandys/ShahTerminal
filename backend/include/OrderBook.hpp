#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dom
{
    struct Level
    {
        double price{};
        double bidQuantity{};
        double askQuantity{};
    };

    class OrderBook
    {
    public:
        using Tick = std::int64_t;

        OrderBook();

        void clear();

        // Set tick size (price step) in quote currency.
        void setTickSize(double tickSize);

        // Snapshot from REST depth, prices in ticks.
        void loadSnapshot(const std::vector<std::pair<Tick, double>>& bids,
                          const std::vector<std::pair<Tick, double>>& asks);

        // Incremental updates from aggre.depth stream, prices in ticks.
        void applyDelta(const std::vector<std::pair<Tick, double>>& bids,
                        const std::vector<std::pair<Tick, double>>& asks);

        [[nodiscard]] double bestBid() const;
        [[nodiscard]] double bestAsk() const;
        [[nodiscard]] double tickSize() const;

        [[nodiscard]] std::vector<Level> ladder(std::size_t levelsPerSide) const;

    private:
        using BookSide = std::map<Tick, double, std::less<>>;

        BookSide bids_; // key: tick index, value: qty
        BookSide asks_;
        double tickSize_{0.0};

        // Center of the ladder in ticks; adjusted slowly to avoid jumping.
        mutable Tick centerTick_{0};
        mutable bool hasCenter_{false};

        static void applySide(BookSide& side, const std::vector<std::pair<Tick, double>>& updates);
    };
} // namespace dom

#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>

using OrderId  = uint64_t;
using Price    = int32_t;    // price in integer ticks
using Quantity = uint32_t;

enum class Side { Buy, Sell };

// one resting order; prev/next make it a node in its price level's queue
struct Order {
    OrderId  id;
    Side     side;
    Price    price;
    Quantity qty;            // remaining size
    Order*   prev = nullptr;
    Order*   next = nullptr;
};

// every order resting at one price, oldest (head) to newest (tail)
struct PriceLevel {
    Quantity total_qty = 0;
    Order*   head = nullptr;
    Order*   tail = nullptr;
};

// one aggregated level, handed out by the top-of-book snapshot
struct BookLevel {
    Price    price;
    Quantity qty;
};

class OrderBook {
public:
    // base_tick is the price at index 0; num_ticks is how many levels we reserve
    OrderBook(Price base_tick, std::size_t num_ticks);

    void add(OrderId id, Side side, Price price, Quantity qty);
    bool cancel(OrderId id);                   // false if id isn't resting
    bool execute(OrderId id, Quantity qty);    // reduce order `id`; false if not resting

    bool     has_bid() const { return best_bid_idx_ >= 0; }
    bool     has_ask() const { return best_ask_idx_ >= 0; }
    Price    best_bid() const { return base_tick_ + best_bid_idx_; }
    Price    best_ask() const { return base_tick_ + best_ask_idx_; }
    Quantity best_bid_qty() const { return bids_[best_bid_idx_].total_qty; }
    Quantity best_ask_qty() const { return asks_[best_ask_idx_].total_qty; }

    Quantity qty_at(Side side, Price price) const;
    std::size_t dropped() const { return dropped_; }  // orders skipped as out of range
    const Order* find(OrderId id) const;              // resting order, or nullptr

    // fill out[0..n) with the n best levels (best first); returns how many were live
    int top_bids(BookLevel* out, int n) const;
    int top_asks(BookLevel* out, int n) const;

private:
    int  index(Price p) const { return p - base_tick_; }
    bool in_range(int i) const { return i >= 0 && i < static_cast<int>(bids_.size()); }
    void remove(Order& o);          // unlink from its level, fix total + best
    void rescan_best_bid();         // best bid emptied, walk down to next live level
    void rescan_best_ask();

    std::vector<PriceLevel> bids_, asks_;        // indexed by price - base_tick_
    std::unordered_map<OrderId, Order> orders_;  // id -> order storage (stable addrs)
    int   best_bid_idx_ = -1;                    // -1 means empty
    int   best_ask_idx_ = -1;
    Price base_tick_;
    std::size_t dropped_ = 0;                    // count of out-of-range adds
};

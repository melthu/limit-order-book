#pragma once
#include "order_book.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

// Adapter for a Binance partial-depth (depth20) JSON snapshot stream. A crypto
// depth feed is level-based (price -> aggregate size), so we represent each live
// level as one synthetic order and keep the book in sync with add/cancel — the
// book itself stays purely order-based, unaware of the feed.
class CryptoFeed {
public:
    explicit CryptoFeed(OrderBook& book) : book_(book) {}

    // sync the book to one depth snapshot; returns false if it couldn't parse
    bool apply(const std::string& json);

private:
    struct Level { OrderId id; Quantity qty; uint64_t seen; };

    OrderBook& book_;
    std::unordered_map<int64_t, Level> levels_;   // (side,price) -> synthetic order
    OrderId  next_id_ = 1;
    uint64_t gen_ = 0;

    void sync_level(Side side, Price price, Quantity qty);
};

// best bid of a snapshot in integer ticks (cents), or -1 — used to size the book
long peek_top_bid(const std::string& json);

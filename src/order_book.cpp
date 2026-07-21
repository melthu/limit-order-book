#include "order_book.hpp"

OrderBook::OrderBook(Price base_tick, std::size_t num_ticks)
    : bids_(num_ticks), asks_(num_ticks), base_tick_(base_tick) {}

void OrderBook::add(OrderId id, Side side, Price price, Quantity qty) {
    int i = index(price);
    auto& book = (side == Side::Buy) ? bids_ : asks_;
    PriceLevel& lvl = book[i];

    // store it in the map first so its address stays put for the list pointers
    Order& o = orders_[id];
    o.id = id; o.side = side; o.price = price; o.qty = qty;

    // append to the back of the level's queue (newest fills last)
    o.prev = lvl.tail;
    o.next = nullptr;
    if (lvl.tail) lvl.tail->next = &o;
    else          lvl.head = &o;      // level was empty
    lvl.tail = &o;

    lvl.total_qty += qty;

    // did this order set a new top of book?
    if (side == Side::Buy) {
        if (i > best_bid_idx_) best_bid_idx_ = i;   // highest bid wins
    } else {
        if (best_ask_idx_ < 0 || i < best_ask_idx_) best_ask_idx_ = i;  // lowest ask wins
    }
}

void OrderBook::cancel(OrderId id) {
    // TODO
}

void OrderBook::execute(OrderId id, Quantity qty) {
    // TODO
}

Quantity OrderBook::qty_at(Side side, Price price) const {
    const auto& book = (side == Side::Buy) ? bids_ : asks_;
    return book[index(price)].total_qty;
}

void OrderBook::remove(Order& o) {
    // TODO
}

void OrderBook::rescan_best_bid() {
    // TODO
}

void OrderBook::rescan_best_ask() {
    // TODO
}

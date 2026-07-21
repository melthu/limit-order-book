#include "order_book.hpp"

OrderBook::OrderBook(Price base_tick, std::size_t num_ticks)
    : bids_(num_ticks), asks_(num_ticks), base_tick_(base_tick) {}

void OrderBook::add(OrderId id, Side side, Price price, Quantity qty) {
    // TODO
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

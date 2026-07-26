#include "order_book.hpp"

OrderBook::OrderBook(Price base_tick, std::size_t num_ticks, std::size_t expected_orders)
    : bids_(num_ticks), asks_(num_ticks), base_tick_(base_tick) {
    orders_.reserve(expected_orders);   // avoid rehashing while the book fills up
}

void OrderBook::add(OrderId id, Side side, Price price, Quantity qty) {
    int i = index(price);
    if (!in_range(i)) { ++dropped_; return; }   // price outside our band, skip it

    // store it in the map first so its address stays put for the list pointers.
    // a duplicate id would repoint an already-linked node and corrupt its old
    // level, so ignore it — a clean feed never re-adds a live id.
    auto [it, inserted] = orders_.try_emplace(id);
    if (!inserted) { ++dropped_; return; }
    Order& o = it->second;
    o.id = id; o.side = side; o.price = price; o.qty = qty;

    auto& book = (side == Side::Buy) ? bids_ : asks_;
    PriceLevel& lvl = book[i];

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

bool OrderBook::cancel(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;   // unknown id, nothing to do
    remove(it->second);
    orders_.erase(it);
    return true;
}

bool OrderBook::execute(OrderId id, Quantity qty) {
    auto it = orders_.find(id);
    if (it == orders_.end()) return false;   // unknown id, nothing to do
    Order& o = it->second;

    if (qty >= o.qty) {                // fully filled -> drop the order
        remove(o);
        orders_.erase(it);
    } else {                          // partial fill -> just shrink it
        o.qty -= qty;
        auto& book = (o.side == Side::Buy) ? bids_ : asks_;
        book[index(o.price)].total_qty -= qty;
    }
    return true;
}

const Order* OrderBook::find(OrderId id) const {
    auto it = orders_.find(id);
    return (it == orders_.end()) ? nullptr : &it->second;
}

Quantity OrderBook::qty_at(Side side, Price price) const {
    int i = index(price);
    if (!in_range(i)) return 0;
    const auto& book = (side == Side::Buy) ? bids_ : asks_;
    return book[i].total_qty;
}

int OrderBook::top_bids(BookLevel* out, int n) const {
    int c = 0;
    for (int i = best_bid_idx_; i >= 0 && c < n; --i) {   // best bid = highest price
        if (bids_[i].head) out[c++] = { base_tick_ + i, bids_[i].total_qty };
    }
    return c;
}

int OrderBook::top_asks(BookLevel* out, int n) const {
    int c = 0;
    int sz = static_cast<int>(asks_.size());
    for (int i = best_ask_idx_; i >= 0 && i < sz && c < n; ++i) {  // best ask = lowest price
        if (asks_[i].head) out[c++] = { base_tick_ + i, asks_[i].total_qty };
    }
    return c;
}

void OrderBook::remove(Order& o) {
    auto& book = (o.side == Side::Buy) ? bids_ : asks_;
    PriceLevel& lvl = book[index(o.price)];

    // splice out of the doubly linked list
    if (o.prev) o.prev->next = o.next;
    else        lvl.head = o.next;     // was the head
    if (o.next) o.next->prev = o.prev;
    else        lvl.tail = o.prev;     // was the tail

    lvl.total_qty -= o.qty;

    // if the best level just emptied, walk to the next live one
    if (lvl.head == nullptr) {
        if (o.side == Side::Buy  && index(o.price) == best_bid_idx_) rescan_best_bid();
        if (o.side == Side::Sell && index(o.price) == best_ask_idx_) rescan_best_ask();
    }
}

void OrderBook::rescan_best_bid() {
    int i = best_bid_idx_;
    while (i >= 0 && bids_[i].head == nullptr) --i;
    best_bid_idx_ = i;                 // -1 if the whole side is empty
}

void OrderBook::rescan_best_ask() {
    int i = best_ask_idx_;
    int n = static_cast<int>(asks_.size());
    while (i < n && asks_[i].head == nullptr) ++i;
    best_ask_idx_ = (i < n) ? i : -1;
}

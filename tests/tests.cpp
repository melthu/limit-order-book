#include "order_book.hpp"
#include <cassert>
#include <cstdio>

int main() {
    // book covering ticks 10000..10999
    OrderBook book(10000, 1000);

    // nothing added yet, so both sides are empty
    assert(!book.has_bid());
    assert(!book.has_ask());

    // build a small book
    book.add(1, Side::Buy, 10005, 100);
    book.add(2, Side::Buy, 10005, 50);    // same price, queues behind #1
    book.add(3, Side::Buy, 10003, 200);   // worse bid
    book.add(4, Side::Sell, 10008, 75);
    book.add(5, Side::Sell, 10010, 80);   // worse ask

    assert(book.has_bid() && book.has_ask());
    assert(book.best_bid() == 10005);              // highest bid is best
    assert(book.best_bid_qty() == 150);            // 100 + 50 resting there
    assert(book.best_ask() == 10008);              // lowest ask is best
    assert(book.best_ask_qty() == 75);
    assert(book.qty_at(Side::Buy, 10003) == 200);  // the worse bid level

    std::puts("add ok");
    return 0;
}

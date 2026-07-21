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

    // cancels
    book.add(6, Side::Buy, 10005, 30);    // 10005 queue is now #1, #2, #6
    assert(book.best_bid_qty() == 180);

    book.cancel(2);                        // unlink from the middle of the queue
    assert(book.best_bid_qty() == 130);    // 100 + 30 remain
    assert(book.best_bid() == 10005);

    book.cancel(1);                        // unlink the head
    assert(book.best_bid_qty() == 30);

    book.cancel(6);                        // last order gone -> best bid drops a level
    assert(book.best_bid() == 10003);
    assert(book.best_bid_qty() == 200);

    book.cancel(4);                        // best ask level empties -> ask drops
    assert(book.best_ask() == 10010);
    assert(book.best_ask_qty() == 80);

    book.cancel(999);                      // unknown id is a no-op
    assert(book.best_bid() == 10003);
    assert(book.best_ask() == 10010);

    // executions
    book.add(7, Side::Sell, 10009, 40);    // new best ask
    assert(book.best_ask() == 10009 && book.best_ask_qty() == 40);

    book.execute(7, 10);                   // partial fill, order stays
    assert(book.best_ask() == 10009 && book.best_ask_qty() == 30);

    book.execute(7, 30);                   // fully filled -> level empties, ask drops
    assert(book.best_ask() == 10010 && book.best_ask_qty() == 80);

    book.add(8, Side::Buy, 10004, 50);     // 10004 queue is #8 then #9
    book.add(9, Side::Buy, 10004, 60);
    assert(book.best_bid() == 10004 && book.best_bid_qty() == 110);

    book.execute(8, 50);                   // head fills first (price-time priority)
    assert(book.best_bid() == 10004 && book.best_bid_qty() == 60);

    book.execute(9, 60);                   // level empties -> best bid drops
    assert(book.best_bid() == 10003 && book.best_bid_qty() == 200);

    book.execute(3, 50);                   // partial fill of the last bid
    assert(book.best_bid_qty() == 150);

    book.execute(999, 5);                  // unknown id is a no-op
    assert(book.best_bid() == 10003 && book.best_ask() == 10010);

    book.execute(3, 1000);                 // over-fill clamps to full removal
    assert(!book.has_bid());               // bid side is now empty
    assert(book.best_ask() == 10010);      // ask side untouched

    std::puts("add + cancel + execute ok");
    return 0;
}

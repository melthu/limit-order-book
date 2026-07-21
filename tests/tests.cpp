#include "order_book.hpp"
#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);  \
            ++failures;                                                  \
        }                                                                \
    } while (0)

static void test_add() {
    OrderBook book(10000, 1000);
    CHECK(!book.has_bid());
    CHECK(!book.has_ask());

    book.add(1, Side::Buy, 10005, 100);
    book.add(2, Side::Buy, 10005, 50);    // same price, queues behind #1
    book.add(3, Side::Buy, 10003, 200);   // worse bid
    book.add(4, Side::Sell, 10008, 75);
    book.add(5, Side::Sell, 10010, 80);   // worse ask

    CHECK(book.has_bid() && book.has_ask());
    CHECK(book.best_bid() == 10005);              // highest bid is best
    CHECK(book.best_bid_qty() == 150);            // 100 + 50 resting there
    CHECK(book.best_ask() == 10008);              // lowest ask is best
    CHECK(book.best_ask_qty() == 75);
    CHECK(book.qty_at(Side::Buy, 10003) == 200);  // the worse bid level
}

static void test_cancel() {
    OrderBook book(10000, 1000);
    book.add(1, Side::Buy, 10005, 100);
    book.add(2, Side::Buy, 10005, 50);
    book.add(3, Side::Buy, 10003, 200);
    book.add(4, Side::Sell, 10008, 75);
    book.add(5, Side::Sell, 10010, 80);
    book.add(6, Side::Buy, 10005, 30);    // 10005 queue is now #1, #2, #6
    CHECK(book.best_bid_qty() == 180);

    book.cancel(2);                        // unlink from the middle of the queue
    CHECK(book.best_bid_qty() == 130);     // 100 + 30 remain
    CHECK(book.best_bid() == 10005);

    book.cancel(1);                        // unlink the head
    CHECK(book.best_bid_qty() == 30);

    book.cancel(6);                        // last order gone -> best bid drops a level
    CHECK(book.best_bid() == 10003);
    CHECK(book.best_bid_qty() == 200);

    book.cancel(4);                        // best ask level empties -> ask drops
    CHECK(book.best_ask() == 10010);
    CHECK(book.best_ask_qty() == 80);

    book.cancel(999);                      // unknown id is a no-op
    CHECK(book.best_bid() == 10003);
    CHECK(book.best_ask() == 10010);
}

static void test_execute() {
    OrderBook book(10000, 1000);
    book.add(3, Side::Buy, 10003, 200);
    book.add(5, Side::Sell, 10010, 80);

    book.add(7, Side::Sell, 10009, 40);    // new best ask
    CHECK(book.best_ask() == 10009 && book.best_ask_qty() == 40);

    book.execute(7, 10);                   // partial fill, order stays
    CHECK(book.best_ask() == 10009 && book.best_ask_qty() == 30);

    book.execute(7, 30);                   // fully filled -> level empties, ask drops
    CHECK(book.best_ask() == 10010 && book.best_ask_qty() == 80);

    book.add(8, Side::Buy, 10004, 50);     // 10004 queue is #8 then #9
    book.add(9, Side::Buy, 10004, 60);
    CHECK(book.best_bid() == 10004 && book.best_bid_qty() == 110);

    book.execute(8, 50);                   // head fills first (price-time priority)
    CHECK(book.best_bid() == 10004 && book.best_bid_qty() == 60);

    book.execute(9, 60);                   // level empties -> best bid drops
    CHECK(book.best_bid() == 10003 && book.best_bid_qty() == 200);

    book.execute(3, 50);                   // partial fill of the last bid
    CHECK(book.best_bid_qty() == 150);

    book.execute(999, 5);                  // unknown id is a no-op
    CHECK(book.best_bid() == 10003 && book.best_ask() == 10010);

    book.execute(3, 1000);                 // over-fill clamps to full removal
    CHECK(!book.has_bid());                // bid side is now empty
    CHECK(book.best_ask() == 10010);       // ask side untouched
}

static void test_bounds() {
    OrderBook book(10000, 10);             // only covers ticks 10000..10009

    book.add(1, Side::Buy, 10005, 50);     // in range
    book.add(2, Side::Buy, 9000, 50);      // below the band -> dropped
    book.add(3, Side::Sell, 20000, 50);    // above the band -> dropped

    CHECK(book.dropped() == 2);
    CHECK(book.best_bid() == 10005);
    CHECK(book.best_bid_qty() == 50);
    CHECK(!book.has_ask());                 // the only ask was out of range
    CHECK(book.qty_at(Side::Buy, 9000) == 0);  // out-of-range query is safe
}

int main() {
    test_add();
    test_cancel();
    test_execute();
    test_bounds();

    if (failures == 0) {
        std::puts("all tests passed");
        return 0;
    }
    std::printf("%d checks failed\n", failures);
    return 1;
}

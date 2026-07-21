#include "order_book.hpp"
#include <cassert>
#include <cstdio>

int main() {
    // book covering ticks 10000..10999
    OrderBook book(10000, 1000);

    // nothing added yet, so both sides are empty
    assert(!book.has_bid());
    assert(!book.has_ask());

    std::puts("scaffold ok");
    return 0;
}

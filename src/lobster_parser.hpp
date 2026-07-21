#pragma once
#include "order_book.hpp"
#include <string>

// one row of a LOBSTER message file
struct LobsterMessage {
    int      type;      // 1 add, 2 partial cancel, 3 delete, 4 execute, 5 hidden exec, 6 cross, 7 halt
    OrderId  id;
    Quantity size;
    Price    price;     // dollars x 10000, straight into the book's tick space
    Side     side;      // from direction: 1 = buy, -1 = sell
};

// parse one comma-separated message line (time, type, id, size, price, direction)
LobsterMessage parse_message(const std::string& line);

// translate a message into book calls; the book never sees the CSV.
// returns false when the event references an order we never saw added
// (i.e. one resting before our stream began) so it can't be reconstructed.
bool apply(OrderBook& book, const LobsterMessage& m);

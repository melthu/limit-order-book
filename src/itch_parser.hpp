#pragma once
#include "order_book.hpp"
#include <cstdint>

// Nasdaq TotalView-ITCH 5.0 adapter. Messages are big-endian binary; every
// message carries a 2-byte stock_locate at offset 1 so we can filter one symbol.
namespace itch {

// stock_locate code, present at offset 1 in every message
uint16_t stock_locate(const uint8_t* msg);

// if msg is a Stock Directory ('R') for `symbol`, return its stock_locate; else 0
uint16_t directory_locate(const uint8_t* msg, const char* symbol);

// apply one order message (A/F/E/C/X/D/U) to the book; other types are ignored.
// returns false if it referenced an order that wasn't resting (parser/feed error).
bool apply(OrderBook& book, const uint8_t* msg);

}  // namespace itch

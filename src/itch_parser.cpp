#include "itch_parser.hpp"

namespace itch {

// big-endian field readers (ITCH integers are network byte order)
static uint16_t u16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | p[1];
}
static uint32_t u32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
static uint64_t u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

uint16_t stock_locate(const uint8_t* msg) {
    return u16(msg + 1);
}

uint16_t directory_locate(const uint8_t* msg, const char* symbol) {
    if (msg[0] != 'R') return 0;
    const uint8_t* stock = msg + 11;   // 8-byte ticker, right-padded with spaces
    bool ended = false;
    for (int i = 0; i < 8; ++i) {
        if (!ended && symbol[i] == '\0') ended = true;
        char want = ended ? ' ' : symbol[i];
        if (char(stock[i]) != want) return 0;
    }
    return u16(msg + 1);
}

// field offsets are from the ITCH 5.0 spec; offset 0 is the message type byte
bool apply(OrderBook& book, const uint8_t* m) {
    switch (m[0]) {
        case 'A':   // Add Order (no MPID)
        case 'F': { // Add Order with MPID Attribution (same layout up to price)
            OrderId  ref = u64(m + 11);
            Side     side = (m[19] == 'B') ? Side::Buy : Side::Sell;
            Quantity sh  = u32(m + 20);
            Price    px  = Price(u32(m + 32));
            book.add(ref, side, px, sh);
            return true;
        }
        case 'E':   // Order Executed
        case 'C':   // Order Executed with Price (shares still at offset 19)
        case 'X':   // Order Cancel (partial) — canceled_shares at offset 19
            return book.execute(u64(m + 11), u32(m + 19));
        case 'D':   // Order Delete (full)
            return book.cancel(u64(m + 11));
        case 'U': { // Order Replace = delete original, add new (inherits side)
            OrderId orig = u64(m + 11);
            const Order* o = book.find(orig);
            if (!o) return false;
            Side side = o->side;
            OrderId  neu = u64(m + 19);
            Quantity sh  = u32(m + 27);
            Price    px  = Price(u32(m + 31));
            book.cancel(orig);
            book.add(neu, side, px, sh);
            return true;
        }
        default:    // non-book message
            return true;
    }
}

}  // namespace itch

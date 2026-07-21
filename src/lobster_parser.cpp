#include "lobster_parser.hpp"
#include <cstdlib>

LobsterMessage parse_message(const std::string& line) {
    LobsterMessage m{};
    const char* p = line.c_str();
    char* end;

    std::strtod(p, &end);                     // time — not needed for the book
    p = end + 1;
    m.type  = std::strtol(p, &end, 10);   p = end + 1;
    m.id    = std::strtoull(p, &end, 10); p = end + 1;
    m.size  = std::strtoul(p, &end, 10);  p = end + 1;
    m.price = std::strtol(p, &end, 10);   p = end + 1;
    int dir = std::strtol(p, &end, 10);
    m.side  = (dir == 1) ? Side::Buy : Side::Sell;
    return m;
}

bool apply(OrderBook& book, const LobsterMessage& m) {
    switch (m.type) {
        case 1: book.add(m.id, m.side, m.price, m.size); return true;
        case 2:                                 // partial cancel = reduce
        case 4: return book.execute(m.id, m.size);   // visible execution = reduce
        case 3: return book.cancel(m.id);            // full delete
        // 5 hidden execution, 6 cross, 7 halt: no effect on the visible book
        default: return true;
    }
}

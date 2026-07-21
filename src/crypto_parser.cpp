#include "crypto_parser.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

// prices come as strings like "66434.58000000"; store as cents (tick 0.01)
static Price to_price(double p) { return Price(std::llround(p * 100.0)); }
// sizes are fractional coin like "0.03176000"; scale to micro-units to fit an int
static Quantity to_qty(double q) { return Quantity(std::llround(q * 1e6)); }

static int64_t key_of(Side side, Price price) {
    return int64_t(price) | (int64_t(side == Side::Sell) << 40);
}

// point just past `key` in s, or nullptr
static const char* after(const char* s, const char* key) {
    const char* p = std::strstr(s, key);
    return p ? p + std::strlen(key) : nullptr;
}

void CryptoFeed::sync_level(Side side, Price price, Quantity qty) {
    int64_t k = key_of(side, price);
    auto it = levels_.find(k);
    if (it == levels_.end()) {                 // new level
        OrderId id = next_id_++;
        book_.add(id, side, price, qty);
        levels_[k] = { id, qty, gen_ };
    } else if (it->second.qty != qty) {        // size changed -> replace
        book_.cancel(it->second.id);
        OrderId id = next_id_++;
        book_.add(id, side, price, qty);
        it->second = { id, qty, gen_ };
    } else {                                   // unchanged
        it->second.seen = gen_;
    }
}

bool CryptoFeed::apply(const std::string& json) {
    const char* b = after(json.c_str(), "\"bids\":[");
    const char* a = after(json.c_str(), "\"asks\":[");
    if (!b || !a) return false;

    ++gen_;
    const char* sides[2] = { b, a };
    for (int s = 0; s < 2; ++s) {
        Side side = (s == 0) ? Side::Buy : Side::Sell;
        const char* p = sides[s];
        while (*p && *p != ']') {                   // walk ["price","qty"] pairs
            if (*p != '[') { ++p; continue; }
            ++p;
            if (*p == '"') ++p;
            char* end;
            double pr = std::strtod(p, &end); p = end;
            while (*p && *p != ',') ++p;             // skip to comma
            if (*p) ++p;
            while (*p && *p != '"') ++p;             // to qty opening quote
            if (*p == '"') ++p;
            double q = std::strtod(p, &end); p = end;
            while (*p && *p != ']') ++p;             // to this pair's ]
            if (*p == ']') ++p;
            Quantity qty = to_qty(q);
            if (qty > 0) sync_level(side, to_price(pr), qty);
        }
    }

    // drop levels that weren't in this snapshot
    for (auto it = levels_.begin(); it != levels_.end();) {
        if (it->second.seen != gen_) {
            book_.cancel(it->second.id);
            it = levels_.erase(it);
        } else {
            ++it;
        }
    }
    return true;
}

long peek_top_bid(const std::string& json) {
    const char* p = after(json.c_str(), "\"bids\":[[\"");
    if (!p) return -1;
    return long(to_price(std::strtod(p, nullptr)));
}

#pragma once
#include "order_book.hpp"

struct Signal {
    double mid;        // (Pb + Pa) / 2, in ticks
    double imbalance;  // (Qb - Qa) / (Qb + Qa), in [-1, +1]
    double micro_dev;  // microprice - mid (size-weighted fair value, relative to mid)
    double ofi;        // order-flow imbalance contribution for this event
};

// Top-of-book microstructure signals, event to event. Imbalance and microprice
// are snapshots; OFI needs the previous touch, so feed update() each time the top
// of book changes. Both sides must be non-empty (Qb, Qa > 0).
class Signals {
public:
    Signal update(Price bid, Quantity bid_qty, Price ask, Quantity ask_qty);
    bool has_prev() const { return have_prev_; }

private:
    Price    pb_ = 0, pa_ = 0;
    Quantity qb_ = 0, qa_ = 0;
    bool     have_prev_ = false;
};

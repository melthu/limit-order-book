#include "signals.hpp"

Signal Signals::update(Price bid, Quantity bid_qty, Price ask, Quantity ask_qty) {
    double qb = bid_qty, qa = ask_qty;
    double sum = qb + qa;

    Signal s;
    s.mid       = (double(bid) + double(ask)) / 2.0;
    s.imbalance = (qb - qa) / sum;
    double micro = (double(ask) * qb + double(bid) * qa) / sum;   // size-weighted mid
    s.micro_dev = micro - s.mid;

    // OFI: net signed order flow at the touch vs the previous state (Cont et al. 2014)
    if (!have_prev_) {
        s.ofi = 0.0;
    } else {
        double eb;
        if      (bid > pb_)  eb = qb;                  // higher bid = buying pressure
        else if (bid == pb_) eb = qb - double(qb_);    // same price, size change
        else                 eb = -double(qb_);        // bid pulled/hit
        double ea;
        if      (ask > pa_)  ea = -double(qa_);        // ask lifted/pulled
        else if (ask == pa_) ea = qa - double(qa_);
        else                 ea = qa;                  // lower ask = selling pressure
        s.ofi = eb - ea;
    }

    pb_ = bid; qb_ = bid_qty;
    pa_ = ask; qa_ = ask_qty;
    have_prev_ = true;
    return s;
}

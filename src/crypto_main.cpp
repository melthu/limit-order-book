#include "order_book.hpp"
#include "crypto_parser.hpp"
#include "signals.hpp"
#include <cstdio>
#include <iostream>
#include <string>

// BTC in cents; band spans +/- $10k around the opening price
static const std::size_t TICKS = 2000000;
static const long        BAND  = 1000000;   // cents below first bid for base
static const int         LEVELS = 10;       // ladder depth for the dashboard

// dump top-of-book + signals for the python dashboard, one line per snapshot.
// D <mid> <imb> <micro_dev> <ofi>  B <p> <q> ...(10)  A <p> <q> ...(10)
// prices/mid in dollars, sizes in BTC, ofi in BTC, micro_dev in dollars.
static void emit_line(const OrderBook& book, const Signal& s) {
    std::printf("D %.2f %.4f %.4f %.6f", s.mid / 100.0, s.imbalance,
                s.micro_dev / 100.0, s.ofi / 1e6);
    BookLevel lv[LEVELS];
    int nb = book.top_bids(lv, LEVELS);
    std::printf(" B");
    for (int i = 0; i < nb; ++i) std::printf(" %.2f %.4f", lv[i].price / 100.0, lv[i].qty / 1e6);
    int na = book.top_asks(lv, LEVELS);
    std::printf(" A");
    for (int i = 0; i < na; ++i) std::printf(" %.2f %.4f", lv[i].price / 100.0, lv[i].qty / 1e6);
    std::printf("\n");
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    bool emit = argc > 1 && std::string(argv[1]) == "emit";   // dashboard stream vs human print
    int every = emit ? 1 : 10;                                // emit every 100ms; human ~1s

    std::string line;
    if (!std::getline(std::cin, line)) { std::fprintf(stderr, "no input\n"); return 1; }

    long top = peek_top_bid(line);
    if (top < 0) { std::fprintf(stderr, "bad first snapshot\n"); return 1; }
    Price base = Price(top - BAND);

    OrderBook book(base, TICKS);
    CryptoFeed feed(book);
    Signals sig;

    long n = 0;
    do {
        if (!feed.apply(line)) continue;
        if (++n % every != 0) continue;
        if (!book.has_bid() || !book.has_ask()) continue;

        // same signal module as the historical ITCH study, on a live feed
        Signal s = sig.update(book.best_bid(), book.best_bid_qty(),
                              book.best_ask(), book.best_ask_qty());
        if (emit) { emit_line(book, s); continue; }

        double pb = book.best_bid() / 100.0, pa = book.best_ask() / 100.0;
        double qb = book.best_bid_qty() / 1e6, qa = book.best_ask_qty() / 1e6;
        double micro = (s.mid + s.micro_dev) / 100.0;     // cents -> dollars
        std::printf("bid %.2f x%.4f | ask %.2f x%.4f | spread %.2f | imb %+.3f | micro %.2f | ofi %+.3f\n",
                    pb, qb, pa, qa, pa - pb, s.imbalance, micro, s.ofi / 1e6);
        std::fflush(stdout);
    } while (std::getline(std::cin, line));

    return 0;
}

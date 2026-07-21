#include "order_book.hpp"
#include "crypto_parser.hpp"
#include <cstdio>
#include <iostream>
#include <string>

// BTC in cents; band spans +/- $10k around the opening price
static const std::size_t TICKS = 2000000;
static const long        BAND  = 1000000;   // cents below first bid for base
static const int         EVERY = 10;        // print every ~1s (10 x 100ms)

int main() {
    std::string line;
    if (!std::getline(std::cin, line)) { std::fprintf(stderr, "no input\n"); return 1; }

    long top = peek_top_bid(line);
    if (top < 0) { std::fprintf(stderr, "bad first snapshot\n"); return 1; }
    Price base = Price(top - BAND);

    OrderBook book(base, TICKS);
    CryptoFeed feed(book);

    long n = 0;
    do {
        if (!feed.apply(line)) continue;
        if (++n % EVERY != 0) continue;
        if (!book.has_bid() || !book.has_ask()) continue;

        double pb = book.best_bid() / 100.0, pa = book.best_ask() / 100.0;
        double qb = book.best_bid_qty() / 1e6, qa = book.best_ask_qty() / 1e6;
        double imb = (qb - qa) / (qb + qa);
        double micro = (pa * qb + pb * qa) / (qb + qa);   // size-weighted fair value
        std::printf("bid %.2f x%.4f | ask %.2f x%.4f | spread %.2f | imb %+.3f | micro %.2f\n",
                    pb, qb, pa, qa, pa - pb, imb, micro);
        std::fflush(stdout);
    } while (std::getline(std::cin, line));

    return 0;
}

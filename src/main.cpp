#include "order_book.hpp"
#include "lobster_parser.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

// AAPL 2012-06-21 traded ~$577..$588; size the band generously ($570..$600)
static const Price       BASE   = 5700000;
static const std::size_t TICKS  = 300000;
static const int         LEVELS = 10;

// LOBSTER marks unoccupied levels with +/-9999999999; real prices are ~5.8e6
static bool is_empty(long price) { return price > 1000000000L || price < -1000000000L; }

// the shown levels of one side, best first
struct SideLevels {
    Price price[LEVELS];
    long  size[LEVELS];
    int   count = 0;
    bool  full  = false;   // all LEVELS shown -> deeper levels exist but are hidden
};

static void parse_book(const std::string& line, SideLevels& asks, SideLevels& bids) {
    asks.count = bids.count = 0;
    const char* p = line.c_str();
    char* end;
    for (int lvl = 0; lvl < LEVELS; ++lvl) {
        long ap = std::strtol(p, &end, 10); p = end + 1;
        long as = std::strtol(p, &end, 10); p = end + 1;
        long bp = std::strtol(p, &end, 10); p = end + 1;
        long bs = std::strtol(p, &end, 10); p = (*end == ',') ? end + 1 : end;
        if (!is_empty(ap)) { asks.price[asks.count] = ap; asks.size[asks.count] = as; ++asks.count; }
        if (!is_empty(bp)) { bids.price[bids.count] = bp; bids.size[bids.count] = bs; ++bids.count; }
    }
    asks.full = (asks.count == LEVELS);
    bids.full = (bids.count == LEVELS);
}

// size resting at price p on this side; known=false if p is deeper than the shown window
static long level_size(const SideLevels& s, Side side, Price p, bool& known) {
    for (int i = 0; i < s.count; ++i)
        if (s.price[i] == p) { known = true; return s.size[i]; }
    if (!s.full) { known = true; return 0; }        // whole side visible, empty here
    Price worst = s.price[s.count - 1];              // deepest shown level
    known = (side == Side::Buy) ? (p >= worst) : (p <= worst);
    return 0;
}

int main(int argc, char** argv) {
    const char* msg_path = (argc > 1) ? argv[1]
        : "data/AAPL_2012-06-21_34200000_57600000_message_10.csv";
    const char* ob_path = (argc > 2) ? argv[2]
        : "data/AAPL_2012-06-21_34200000_57600000_orderbook_10.csv";

    std::ifstream msg(msg_path), ob(ob_path);
    if (!msg || !ob) { std::fprintf(stderr, "cannot open input files\n"); return 1; }

    OrderBook book(BASE, TICKS);
    SideLevels prev_a, prev_b, cur_a, cur_b;
    std::string mline, oline;

    // row 1 is the state after message 1; keep it as the baseline and skip that message
    if (!std::getline(ob, oline)) return 1;
    parse_book(oline, prev_a, prev_b);
    std::getline(msg, mline);

    long events = 0, prewindow = 0, covered = 0, correct = 0, first_bad = -1, row = 1;

    while (std::getline(msg, mline) && std::getline(ob, oline)) {
        ++row;
        ++events;
        parse_book(oline, cur_a, cur_b);
        LobsterMessage m = parse_message(mline);

        // our engine's change to the affected level, before/after applying
        long before = book.qty_at(m.side, m.price);
        bool reconstructable = apply(book, m);
        long after = book.qty_at(m.side, m.price);
        long our_delta = after - before;

        if (!reconstructable) { ++prewindow; }   // references an order resting before our stream
        else {
            bool kp, kc;
            long lp = level_size(m.side == Side::Buy ? prev_b : prev_a, m.side, m.price, kp);
            long lc = level_size(m.side == Side::Buy ? cur_b : cur_a, m.side, m.price, kc);
            if (kp && kc) {                       // price visible in both snapshots
                ++covered;
                if (our_delta == lc - lp) ++correct;
                else if (first_bad < 0) first_bad = row;
            }
        }

        prev_a = cur_a;
        prev_b = cur_b;
    }

    std::printf("events:              %ld\n", events);
    std::printf("pre-window (skip):   %ld  (%.2f%%)\n", prewindow, 100.0 * prewindow / events);
    std::printf("reconstructable, in-window: %ld\n", covered);
    std::printf("  delta correct:     %ld  (%.4f%%)\n", correct, 100.0 * correct / covered);
    std::printf("first mismatch at row: %ld\n", first_bad);
    return 0;
}

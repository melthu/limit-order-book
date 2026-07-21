#include "signals.hpp"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

// reads a day of Binance USD-M futures bookTicker CSV from stdin and writes a
// 100ms-sampled signals CSV to stdout. bookTicker is L1 top-of-book, so no book
// to rebuild — rows go straight into Signals::update.
//
//   unzip -p data/bookticker/BTCUSDT-bookTicker-2024-03-15.zip | ./build/bookticker > out.csv
//
// input cols:  update_id,best_bid_price,best_bid_qty,best_ask_price,best_ask_qty,transaction_time,event_time
// output cols: ts,bid,ask,bid_qty,ask_qty,mid,spread,imbalance,micro_dev,ofi
//   ts ms · prices/mid/spread/micro_dev in USD · sizes in BTC · imbalance in [-1,1]
//   ofi = signed order flow summed over the 100ms bucket (BTC units)
//
// the archived files aren't in event order (rows from far-apart times are
// interleaved), which would wreck OFI (it diffs against the previous touch) and
// the bucketing. update_id is the exchange's monotonic sequence number, so we
// buffer the whole day, sort by it to recover true order, then sample. one day
// is ~50M rows -> ~1.6GB of 32-byte rows, fine in RAM.

static const long BUCKET_MS = 100;

struct Row {
    unsigned long long uid;   // update_id — sort key = chronological order
    long long ts;             // transaction_time (ms)
    Price bid, ask;
    Quantity bq, aq;
};

int main() {
    std::vector<Row> rows;
    rows.reserve(60'000'000);

    char* line = nullptr;
    size_t cap = 0;

    // pass 1 — parse every row into memory
    while (getline(&line, &cap, stdin) > 0) {
        if (line[0] < '0' || line[0] > '9') continue;   // header / blank

        char* p = line;
        unsigned long long uid = std::strtoull(p, &p, 10); ++p;
        double bidp = std::strtod(p, &p); ++p;
        double bidq = std::strtod(p, &p); ++p;
        double askp = std::strtod(p, &p); ++p;
        double askq = std::strtod(p, &p); ++p;
        long long txt = std::strtoll(p, &p, 10);   // transaction_time (ms)

        Price bid = Price(std::llround(bidp * 10.0));     // USD -> 0.1 ticks
        Price ask = Price(std::llround(askp * 10.0));
        Quantity bq = Quantity(std::llround(bidq * 1e6)); // BTC -> micro-BTC
        Quantity aq = Quantity(std::llround(askq * 1e6));
        if (bq == 0 || aq == 0 || bid <= 0 || ask <= 0) continue;  // degenerate

        rows.push_back({uid, txt, bid, ask, bq, aq});
    }
    std::free(line);

    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) { return a.uid < b.uid; });

    // pass 2 — walk in event order, sample every 100ms, accumulate OFI per bucket
    Signals sig;
    long long cur_bucket = -1;
    double ofi_acc = 0.0;

    long long L_ts = 0;
    Price L_bid = 0, L_ask = 0;
    Quantity L_bq = 0, L_aq = 0;
    double L_mid = 0, L_imb = 0, L_micro = 0;   // mid/micro in ticks
    bool have = false;

    std::printf("ts,bid,ask,bid_qty,ask_qty,mid,spread,imbalance,micro_dev,ofi\n");

    auto emit = [&]() {
        std::printf("%lld,%.2f,%.2f,%.6f,%.6f,%.2f,%.2f,%.6f,%.4f,%.6f\n",
                    L_ts, L_bid / 10.0, L_ask / 10.0, L_bq / 1e6, L_aq / 1e6,
                    L_mid / 10.0, (L_ask - L_bid) / 10.0, L_imb, L_micro / 10.0,
                    ofi_acc / 1e6);
    };

    long emitted = 0;
    for (const Row& r : rows) {
        long long b = r.ts / BUCKET_MS;
        if (cur_bucket >= 0 && b != cur_bucket && have) {
            emit();
            ++emitted;
            ofi_acc = 0.0;
        }
        cur_bucket = b;

        Signal s = sig.update(r.bid, r.bq, r.ask, r.aq);   // event order -> OFI chains
        ofi_acc += s.ofi;

        L_ts = r.ts; L_bid = r.bid; L_ask = r.ask; L_bq = r.bq; L_aq = r.aq;
        L_mid = s.mid; L_imb = s.imbalance; L_micro = s.micro_dev;
        have = true;
    }
    if (have) { emit(); ++emitted; }

    std::fprintf(stderr, "read %zu rows, wrote %ld\n", rows.size(), emitted);
    return 0;
}

#include "order_book.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

// synthetic single-symbol stream so the timed loops are pure book work
int main() {
    const int N = 1'000'000;
    const Price base = 0;
    const std::size_t ticks = 20000;
    const Price mid = 10000;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> price_d(mid - 500, mid + 500);
    std::uniform_int_distribution<int> qty_d(1, 100);

    std::vector<Price> px(N);
    std::vector<Quantity> qty(N);
    std::vector<Side> side(N);
    for (int i = 0; i < N; ++i) {
        px[i]   = price_d(rng);
        qty[i]  = qty_d(rng);
        side[i] = (rng() & 1) ? Side::Buy : Side::Sell;
    }

    using clk = std::chrono::steady_clock;
    auto now = []{ return clk::now(); };
    auto ns  = [](clk::time_point a, clk::time_point b) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    };

    // clock granularity = smallest nonzero gap between two reads. on apple silicon
    // this is ~42ns (24MHz timer), so any op faster than that reads as 0 or one tick
    // -> per-op p50 is below the timer floor; the tail (p99.9/max) is the real signal.
    long gran = 1'000'000;
    for (int i = 0; i < 100000; ++i) { long d = ns(now(), now()); if (d > 0) gran = std::min<long>(gran, d); }

    OrderBook book(base, ticks, N);   // reserve the id map: all N orders rest at once
    std::vector<long> lat(N);   // per-op timings, preallocated (no alloc in the hot loop)

    // one pass = time each op individually, report aggregate throughput + percentiles
    auto run = [&](const char* name, auto&& op) {
        auto t0 = now();
        for (int i = 0; i < N; ++i) {
            auto a = now();
            op(i);
            lat[i] = ns(a, now());
        }
        double agg = double(ns(t0, now())) / N;              // clean absolute number
        std::sort(lat.begin(), lat.end());
        auto pct = [&](double p){ return lat[std::size_t(p * (N - 1))]; };
        std::printf("%-8s %8.1f %8.1f %8ld %8ld %8ld %8ld\n",
                    name, agg, 1000.0 / agg, pct(0.50), pct(0.99), pct(0.999), lat[N - 1]);
    };

    std::printf("clock granularity: %ld ns  (per-op p50 sits below this floor)\n\n", gran);
    std::printf("%-8s %8s %8s %8s %8s %8s %8s\n",
                "op", "ns/op", "Mops/s", "p50", "p99", "p99.9", "max");

    run("add",     [&](int i){ book.add(i, side[i], px[i], qty[i]); });
    run("execute", [&](int i){ book.execute(i, 1); });   // shave one lot; a few fully fill
    run("cancel",  [&](int i){ book.cancel(i); });

    // read final state so the loops can't be optimized away
    std::printf("\nfinal: has_bid=%d has_ask=%d\n", book.has_bid(), book.has_ask());
    return 0;
}

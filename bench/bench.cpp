#include "order_book.hpp"
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
    auto ns = [](clk::time_point a, clk::time_point b) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
    };

    OrderBook book(base, ticks);

    auto t0 = clk::now();
    for (int i = 0; i < N; ++i) book.add(i, side[i], px[i], qty[i]);
    auto t1 = clk::now();

    // shave one lot off each order (a few fully fill and drop out)
    auto t2 = clk::now();
    for (int i = 0; i < N; ++i) book.execute(i, 1);
    auto t3 = clk::now();

    auto t4 = clk::now();
    for (int i = 0; i < N; ++i) book.cancel(i);
    auto t5 = clk::now();

    struct Row { const char* name; long total; } rows[] = {
        {"add",     ns(t0, t1)},
        {"execute", ns(t2, t3)},
        {"cancel",  ns(t4, t5)},
    };
    std::printf("%-8s %10s %12s\n", "op", "ns/op", "M ops/sec");
    for (auto& r : rows) {
        double per = double(r.total) / N;
        std::printf("%-8s %10.1f %12.1f\n", r.name, per, 1000.0 / per);
    }
    // read final state so the loops can't be optimized away
    std::printf("final: has_bid=%d has_ask=%d\n", book.has_bid(), book.has_ask());
    return 0;
}

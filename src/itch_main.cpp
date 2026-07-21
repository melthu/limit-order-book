#include "order_book.hpp"
#include "itch_parser.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// ITCH prices are 1/10000 dollar; band covers $0..$300 for one liquid name.
// Far-out stub quotes (ITCH allows prices up to $199,999.99) fall outside and
// are dropped by design — a dense array can't span the full range.
static const Price       BASE  = 0;
static const std::size_t TICKS = 3000000;

int main(int argc, char** argv) {
    const char* path   = (argc > 1) ? argv[1] : "data/20190530.BX_ITCH_50";
    const char* symbol = (argc > 2) ? argv[2] : "AAPL";

    int fd = open(path, O_RDONLY);
    if (fd < 0) { std::fprintf(stderr, "cannot open %s\n", path); return 1; }
    struct stat st;
    fstat(fd, &st);
    std::size_t n = st.st_size;
    const uint8_t* data = (const uint8_t*)mmap(nullptr, n, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { std::fprintf(stderr, "mmap failed\n"); return 1; }

    OrderBook book(BASE, TICKS);
    uint16_t target = 0;
    bool have_target = false;
    long total = 0, applied = 0, unknown = 0, crosses = 0;

    using clk = std::chrono::steady_clock;
    auto t0 = clk::now();

    std::size_t off = 0;
    while (off + 2 <= n) {
        uint16_t len = (uint16_t(data[off]) << 8) | data[off + 1];   // big-endian frame length
        off += 2;
        if (len == 0 || off + len > n) break;
        const uint8_t* m = data + off;
        off += len;
        ++total;

        char type = char(m[0]);
        if (type == 'R') {                     // stock directory: find our symbol's locate
            uint16_t loc = itch::directory_locate(m, symbol);
            if (loc) { target = loc; have_target = true; }
            continue;
        }
        if (!have_target) continue;
        if (type != 'A' && type != 'F' && type != 'E' && type != 'C'
            && type != 'X' && type != 'D' && type != 'U') continue;
        if (itch::stock_locate(m) != target) continue;

        ++applied;
        if (!itch::apply(book, m)) ++unknown;
        if (book.has_bid() && book.has_ask() && book.best_bid() >= book.best_ask()) ++crosses;
    }

    auto t1 = clk::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();

    munmap((void*)data, n);
    close(fd);

    std::printf("file:            %s  (%.2f GB)\n", path, n / 1e9);
    std::printf("messages total:  %ld\n", total);
    std::printf("symbol %-8s applied: %ld\n", symbol, applied);
    std::printf("unknown-id refs: %ld   (matches dropped -> far-out orders, not misparse)\n", unknown);
    std::printf("crossed book:    %ld   (should be 0)\n", crosses);
    std::printf("dropped (band):  %zu\n", book.dropped());
    std::printf("parse+build:     %.2fs   (%.1f M msg/s over whole file)\n", secs, total / secs / 1e6);
    return 0;
}

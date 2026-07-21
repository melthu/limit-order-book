#include "order_book.hpp"
#include "itch_parser.hpp"
#include "signals.hpp"
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// replays one symbol from an ITCH file and writes per-event top-of-book signals
// as CSV to stdout: seq,mid,imbalance,micro_dev,ofi. Redirect to a file for the
// Phase-4 analysis (IC / hit-rate / taker P&L in Python).
static const Price       BASE  = 0;
static const std::size_t TICKS = 3000000;

int main(int argc, char** argv) {
    const char* path   = (argc > 1) ? argv[1] : "data/01302019.NASDAQ_ITCH50";
    const char* symbol = (argc > 2) ? argv[2] : "AAPL";

    int fd = open(path, O_RDONLY);
    if (fd < 0) { std::fprintf(stderr, "cannot open %s\n", path); return 1; }
    struct stat st;
    fstat(fd, &st);
    std::size_t n = st.st_size;
    const uint8_t* data = (const uint8_t*)mmap(nullptr, n, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) { std::fprintf(stderr, "mmap failed\n"); return 1; }

    OrderBook book(BASE, TICKS);
    Signals sig;
    uint16_t target = 0;
    bool have_target = false;

    Price lpb = 0, lpa = 0;
    Quantity lqb = 0, lqa = 0;
    bool have_last = false;
    long seq = 0;

    std::printf("ts,mid,imbalance,micro_dev,ofi\n");

    std::size_t off = 0;
    while (off + 2 <= n) {
        uint16_t len = (uint16_t(data[off]) << 8) | data[off + 1];
        off += 2;
        if (len == 0 || off + len > n) break;
        const uint8_t* m = data + off;
        off += len;

        char type = char(m[0]);
        if (type == 'R') {
            uint16_t loc = itch::directory_locate(m, symbol);
            if (loc) { target = loc; have_target = true; }
            continue;
        }
        if (!have_target) continue;
        if (type != 'A' && type != 'F' && type != 'E' && type != 'C'
            && type != 'X' && type != 'D' && type != 'U') continue;
        if (itch::stock_locate(m) != target) continue;

        itch::apply(book, m);
        if (!book.has_bid() || !book.has_ask()) continue;

        Price pb = book.best_bid(), pa = book.best_ask();
        Quantity qb = book.best_bid_qty(), qa = book.best_ask_qty();
        if (have_last && pb == lpb && qb == lqb && pa == lpa && qa == lqa) continue;  // touch unchanged

        uint64_t ts = 0;                                  // 48-bit ns since midnight, offset 5
        for (int i = 0; i < 6; ++i) ts = (ts << 8) | m[5 + i];

        Signal s = sig.update(pb, qb, pa, qa);
        std::printf("%llu,%.1f,%.6f,%.4f,%.1f\n",
                    (unsigned long long)ts, s.mid, s.imbalance, s.micro_dev, s.ofi);
        ++seq;
        lpb = pb; lqb = qb; lpa = pa; lqa = qa; have_last = true;
    }

    munmap((void*)data, n);
    close(fd);
    std::fprintf(stderr, "wrote %ld signal rows for %s\n", seq, symbol);
    return 0;
}

#pragma once

#include "order_book.hpp"
#include "types.hpp"

#include <string>
#include <vector>

namespace exchange {

// ── LevelSnapshot — immutable view of one price level ─────────────────────────
struct LevelSnapshot {
    Price    price{0};
    Quantity total_qty{0};
    uint32_t order_count{0};
};

// ── BookSnapshot — immutable full-book view ───────────────────────────────────
//
// bids: highest price first (best bid at index 0)
// asks: lowest  price first (best ask at index 0)
//
// This is a cold-path type: never allocated in the match loop.
// Construction involves full book traversal and vector allocations — acceptable
// for test validation, debug output, and replay state comparison.
struct BookSnapshot {
    std::vector<LevelSnapshot> bids;
    std::vector<LevelSnapshot> asks;
};

// ── Functions ─────────────────────────────────────────────────────────────────

// Produce a structured snapshot. O(N) where N = total active price levels.
[[nodiscard]] BookSnapshot take_snapshot(const OrderBook& book);

// Produce a human-readable multi-line dump. Example:
//   ASK:
//   103: qty=5  count=1  [id=17 qty=5]
//   BID:
//   100: qty=4  count=1  [id=6 qty=4]
//    99: qty=10 count=2  [id=1 qty=8, id=4 qty=2]
[[nodiscard]] std::string dump_book(const OrderBook& book);

}  // namespace exchange
#include "snapshot.hpp"

#include "order.hpp"
#include "order_book.hpp"
#include "price_level.hpp"

#include <sstream>

namespace exchange {

// ── take_snapshot ─────────────────────────────────────────────────────────────────

BookSnapshot take_snapshot(const OrderBook& book) {
    BookSnapshot snap;

    // Bids: highest price first (best bid at index 0) → reverse iteration
    const auto& bid_levels = book.bids().levels();
    snap.bids.reserve(bid_levels.size());
    for (auto it = bid_levels.rbegin(); it != bid_levels.rend(); ++it) {
        const PriceLevel& lvl = it->second;
        snap.bids.push_back(LevelSnapshot{lvl.price, lvl.total_qty, lvl.order_count});
    }

    // Asks: lowest price first (best ask at index 0) → forward iteration
    const auto& ask_levels = book.asks().levels();
    snap.asks.reserve(ask_levels.size());
    for (const auto& [price, lvl] : ask_levels) {
        snap.asks.push_back(LevelSnapshot{lvl.price, lvl.total_qty, lvl.order_count});
    }

    return snap;
}

// ── dump_book ───────────────────────────────────────────────────────────────────
//
// Output format:
//   ASK:
//     103: qty=5 count=1 [id=17 qty=5]
//   BID:
//     100: qty=4 count=1 [id=6 qty=4]
//      99: qty=10 count=2 [id=1 qty=8, id=4 qty=2]

std::string dump_book(const OrderBook& book) {
    std::ostringstream oss;

    // ── Ask side: lowest price first (best ask at top) ────────────────────
    oss << "ASK:\n";
    const auto& ask_levels = book.asks().levels();
    if (ask_levels.empty()) {
        oss << "  (empty)\n";
    } else {
        for (const auto& [p, lvl] : ask_levels) {
            oss << "  " << lvl.price << ": qty=" << lvl.total_qty
                << " count=" << lvl.order_count << " [";
            const Order* o = lvl.head;
            bool first = true;
            while (o != nullptr) {
                if (!first) oss << ", ";
                oss << "id=" << o->id << " qty=" << o->remaining_qty;
                first = false;
                o = o->next;
            }
            oss << "]\n";
        }
    }

    // ── Bid side: highest price first (best bid at top) ────────────────────
    oss << "BID:\n";
    const auto& bid_levels = book.bids().levels();
    if (bid_levels.empty()) {
        oss << "  (empty)\n";
    } else {
        for (auto it = bid_levels.rbegin(); it != bid_levels.rend(); ++it) {
            const PriceLevel& lvl = it->second;
            oss << "  " << lvl.price << ": qty=" << lvl.total_qty
                << " count=" << lvl.order_count << " [";
            const Order* o = lvl.head;
            bool first = true;
            while (o != nullptr) {
                if (!first) oss << ", ";
                oss << "id=" << o->id << " qty=" << o->remaining_qty;
                first = false;
                o = o->next;
            }
            oss << "]\n";
        }
    }

    return oss.str();
}

}  // namespace exchange
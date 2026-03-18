#pragma once

#include "types.hpp"

#include <cstdint>

namespace exchange {

struct PriceLevel;  // forward declaration — Order stores a back-pointer to its level

// ── Order — exactly one cache line (64 bytes) ─────────────────────────────────
//
// Field ordering is chosen for hot-path locality in the match loop.
// Every iteration of the match loop touches: remaining_qty, id (trade event),
// prev/next (list removal when filled), level (volume update), price (condition).
// All of these land in the first 48 bytes → single cache line, zero spill.
//
// Fields intentionally absent:
//   original_qty — not needed after acceptance; the ORDER_ACCEPTED event carries
//                  it as remaining_qty at creation time. Callers can reconstruct it.
//   type         — market orders never rest on the book; any Order object in the
//                  book is always a limit order by definition.
//
// alignas(64): prepares for a future slab/pool allocator where Orders are packed
// in 64-byte-aligned slots, eliminating false sharing and straddle misses.
struct alignas(64) Order {
    Quantity    remaining_qty{0};   // [0 ] HOTTEST — decremented every match iteration
    OrderId     id{0};              // [8 ] HOT     — emitted in TradeEvent
    Order*      prev{nullptr};      // [16] HOT     — intrusive list removal (O(1) cancel)
    Order*      next{nullptr};      // [24] HOT     — intrusive list removal
    PriceLevel* level{nullptr};     // [32] HOT     — back-ptr; enables O(1) cancel
    Price       price{0};           // [40] HOT     — matching condition + trade price
    SeqNo       seq_no{0};          // [48] warm    — set once at accept; audit / replay
    Side        side{Side::Buy};    // [56] warm    — needed for trade event aggressor side
    OrderState  state{OrderState::Accepted}; // [57] warm — lifecycle transitions
    uint8_t     pad_[6]{};          // [58] explicit padding to reach exactly 64 bytes

    [[nodiscard]] bool is_active() const noexcept {
        return state == OrderState::Active || state == OrderState::PartiallyFilled;
    }

    [[nodiscard]] bool is_fully_filled() const noexcept { return remaining_qty == 0; }
};

static_assert(sizeof(Order) == 64,    "Order must occupy exactly one cache line");
static_assert(alignof(Order) == 64,   "Order must be cache-line aligned");

}  // namespace exchange
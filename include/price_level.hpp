#pragma once

#include "types.hpp"

#include <cstdint>

namespace exchange {

struct Order;  // forward declaration

// ── PriceLevel — 40 bytes ─────────────────────────────────────────────────────
//
// Represents a single price point on one side of the book.
// Orders at this price are stored as an intrusive doubly-linked FIFO list,
// enabling O(1) append (new orders) and O(1) removal (cancel / fill).
//
// Field ordering: head and total_qty are touched every match loop iteration
// (head → next maker, total_qty → decremented on each fill), so they come first.
//
// PriceLevel is stored by VALUE inside std::map nodes. std::map never moves
// a node after insertion, so raw Order* pointers into the list remain stable
// across all map operations including insert and erase on other nodes.
struct PriceLevel {
    explicit PriceLevel(Price p) noexcept : price{p} {}

    // Non-copyable: copying would alias the intrusive Order* pointers.
    PriceLevel(const PriceLevel&)            = delete;
    PriceLevel& operator=(const PriceLevel&) = delete;

    // Movable: std::map needs move for in-place node construction via try_emplace.
    PriceLevel(PriceLevel&&)                 = default;
    PriceLevel& operator=(PriceLevel&&)      = default;

    Order*    head{nullptr};      // [0 ] HOTTEST — first order in FIFO queue (next maker)
    Order*    tail{nullptr};      // [8 ] HOT     — append target for new resting orders
    Quantity  total_qty{0};       // [16] HOT     — decremented each fill; level volume
    Price     price{0};           // [24] warm    — trade price; key for back-lookup via Order::level
    uint32_t  order_count{0};     // [32] cold    — maintained for invariant checks / snapshot
    uint8_t   pad_[4]{};          // [36] explicit padding → sizeof == 40

    // ── Operations ────────────────────────────────────────────────────────────

    // Append o to the tail of the FIFO queue (new resting order).
    // Sets o->level = this and clears o->prev / o->next as needed.
    void append(Order* o) noexcept;

    // Remove o from any position in the FIFO queue (cancel or post-fill cleanup).
    // Clears o->prev, o->next, o->level after removal.
    void remove(Order* o) noexcept;

    [[nodiscard]] bool   empty() const noexcept { return head == nullptr; }
    [[nodiscard]] Order* front() const noexcept { return head; }
};

static_assert(sizeof(PriceLevel) == 40, "PriceLevel layout changed unexpectedly");

}  // namespace exchange
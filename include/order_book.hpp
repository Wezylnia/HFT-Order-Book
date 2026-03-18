#pragma once

#include "order.hpp"
#include "price_level.hpp"
#include "types.hpp"

#include <map>
#include <memory>
#include <unordered_map>

namespace exchange {

// ── BookSide ──────────────────────────────────────────────────────────────────
//
// Holds all active price levels for one side (bids or asks).
//
// Data structure choice: std::map<Price, PriceLevel>
//   - Sorted ascending. Ask best = begin() (lowest ask).
//                        Bid best = rbegin() (highest bid).
//   - map::begin() / rbegin() are O(1) — the tree keeps a cached leftmost
//     pointer, so "get best level" is a single pointer dereference.
//   - insert/erase are O(log N) where N = number of distinct price levels
//     (typically small: a real book rarely has >1000 active levels).
//   - Node-based structure: PriceLevel objects never move after insertion,
//     so Order::level back-pointers remain valid across all map mutations.
//
// Optimization note (post-benchmark): if profiling shows best_level() is a
// bottleneck (e.g. due to map node indirection), add a best_level_cache_ raw
// pointer that is updated on level insert/erase. That converts the cost to a
// single raw pointer dereference with no tree traversal at all.
class BookSide {
public:
    explicit BookSide(Side side) noexcept : side_{side} {}

    // ── Level access ──────────────────────────────────────────────────────────

    // Returns the existing level or creates a new one in-place (no copy/move).
    PriceLevel* get_or_create_level(Price price);

    // Returns the level or nullptr if it does not exist.
    [[nodiscard]] PriceLevel* find_level(Price price) noexcept;

    // Erases a level. Caller must ensure the level is empty first.
    void erase_level(Price price);

    // Returns the best level (lowest ask / highest bid) or nullptr if empty.
    [[nodiscard]] PriceLevel* best_level() noexcept;

    [[nodiscard]] bool empty() const noexcept { return levels_.empty(); }
    [[nodiscard]] Side side()  const noexcept { return side_; }

    // Read-only access for snapshot and invariant checker (cold path only).
    [[nodiscard]] const std::map<Price, PriceLevel>& levels() const noexcept {
        return levels_;
    }

private:
    Side                        side_;
    std::map<Price, PriceLevel> levels_;
};

// ── OrderBook ─────────────────────────────────────────────────────────────────
//
// Central state: two BookSides + an id-keyed ownership map.
//
// Ownership model:
//   - OrderBook owns all live Order objects via unique_ptr in order_index_.
//   - Raw Order* pointers handed out internally are stable for the order's
//     lifetime (unordered_map does not move values; unique_ptr provides a
//     stable heap address).
//   - Ownership is relinquished via unregister_order() when an order is
//     fully filled or cancelled; the caller then decides on destruction.
//
// order_index_ is pre-reserved to 4096 slots on construction. This avoids
// rehashing for typical session sizes and eliminates early allocation spikes.
// Resize threshold: if benchmarks show sessions exceeding ~3000 concurrent
// orders, bump the initial reserve.
class OrderBook {
public:
    explicit OrderBook() { order_index_.reserve(4096); }
    ~OrderBook() = default;

    OrderBook(const OrderBook&)            = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&)                 = default;
    OrderBook& operator=(OrderBook&&)      = default;

    // ── Book placement (does not match) ───────────────────────────────────────

    // Place order onto the appropriate side.
    // Precondition: order is already registered in order_index_.
    void place_on_book(Order* o);

    // Remove order from its price level.
    // Does NOT touch order_index_ — caller decides on lifetime separately.
    void remove_from_book(Order* o);

    // ── Ownership ─────────────────────────────────────────────────────────────

    // Transfer ownership in; returns a stable raw pointer for immediate use.
    Order* register_order(std::unique_ptr<Order> o);

    // Transfer ownership out (order done: filled or cancelled).
    [[nodiscard]] std::unique_ptr<Order> unregister_order(OrderId id);

    // ── Lookup ────────────────────────────────────────────────────────────────

    [[nodiscard]] Order* find(OrderId id) const noexcept;
    [[nodiscard]] bool   has_order(OrderId id) const noexcept;

    // ── Side accessors ────────────────────────────────────────────────────────

    BookSide&       bids() noexcept       { return bids_; }
    BookSide&       asks() noexcept       { return asks_; }
    const BookSide& bids() const noexcept { return bids_; }
    const BookSide& asks() const noexcept { return asks_; }

private:
    BookSide bids_{Side::Buy};
    BookSide asks_{Side::Sell};

    // OrderId → owned Order. Provides duplicate detection, cancel lookup,
    // and lifetime management in a single structure.
    std::unordered_map<OrderId, std::unique_ptr<Order>> order_index_;
};

}  // namespace exchange
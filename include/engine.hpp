#pragma once

#include "command.hpp"
#include "event.hpp"
#include "order_book.hpp"
#include "types.hpp"

namespace exchange {

// ── MatchingEngine ────────────────────────────────────────────────────────────
//
// Single entry point for all order operations. Owns the OrderBook.
//
// Threading model: single-threaded. All submit() calls must come from the
// same thread. No locking, no atomics. This is deliberate: determinism and
// correctness are simpler to reason about and test without concurrency.
//
// Return convention: each submit() returns an EventList by value.
// Rationale: request-scoped vector, owned by caller, no shared mutable state.
// The vector is typically small (< 10 events) and benefits from NRVO/move.
// Pre-allocated capacity (kEventReserve) minimises reallocation for typical ops.
//
// Future optimisation: replace EventList return with a caller-supplied buffer
// or callback to eliminate the vector allocation entirely on the hot path.
class MatchingEngine {
public:
    MatchingEngine()  = default;
    ~MatchingEngine() = default;

    MatchingEngine(const MatchingEngine&)            = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
    MatchingEngine(MatchingEngine&&)                 = default;
    MatchingEngine& operator=(MatchingEngine&&)      = default;

    // ── Public API ────────────────────────────────────────────────────────────

    [[nodiscard]] EventList submit(const NewLimitOrderCmd&  cmd);
    [[nodiscard]] EventList submit(const NewMarketOrderCmd& cmd);
    [[nodiscard]] EventList submit(const CancelOrderCmd&    cmd);
    [[nodiscard]] EventList submit(const ModifyOrderCmd&    cmd);

    // Wipe all state. Used between test cases and replay sessions.
    void reset();

    // Read-only view of book state — for snapshot, tests, invariant checker.
    [[nodiscard]] const OrderBook& book() const noexcept { return book_; }

private:
    // Initial EventList capacity: covers most single-command event bursts
    // (1 accept + up to N trades + 1 terminal state) without reallocation.
    static constexpr std::size_t kEventReserve = 16;

    OrderBook book_;
    SeqNo     next_seq_{0};

    [[nodiscard]] SeqNo advance_seq() noexcept { return next_seq_++; }

    // Core match loop. Runs against the opposite book side until `incoming`
    // is fully filled or no more price-crossing levels remain.
    // All trade and lifecycle events are appended to `out`.
    void match(Order* incoming, EventList& out);

    // Allocate and register a new Order object from limit order fields.
    // Returns the stable raw pointer; ownership stays in book_.
    Order* make_limit_order(const NewLimitOrderCmd& cmd, SeqNo seq);

    // Allocate and register a transient Order for market order processing.
    // Market orders never rest; the Order is unregistered after match().
    Order* make_market_order(const NewMarketOrderCmd& cmd, SeqNo seq);
};

}  // namespace exchange
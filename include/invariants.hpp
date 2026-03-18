#pragma once

#include "order_book.hpp"

namespace exchange {

// ── Invariant checker — debug build only ──────────────────────────────────────
//
// check_invariants() performs a full structural audit of the order book.
// It is gated behind EXCHANGE_DEBUG so it compiles to nothing in Release.
//
// Violations abort via assert() — these are programming errors, not user errors.
// (User errors produce ORDER_REJECTED events; they never reach book state.)
//
// Invariants verified:
//   Crossing check  : best_bid < best_ask (uncrossed book guarantee)
//   Level volumes   : level.total_qty == sum of order.remaining_qty for all orders
//   Level count     : level.order_count == actual FIFO list length
//   List coherence  : head→prev == nullptr; tail→next == nullptr; each node's
//                     prev→next == node and next→prev == node
//   Membership      : every order reachable from a level is also in order_index_
//   Uniqueness      : no order_id appears in more than one level
//   Active state    : every order on the book satisfies is_active() == true
//   Level ptr       : every order's order->level points back to its containing level
//
// Usage: call after each engine operation in tests; gate with EXCHANGE_DEBUG in
// production builds so the hot path sees zero overhead.

#ifdef EXCHANGE_DEBUG
void check_invariants(const OrderBook& book);
#else
inline void check_invariants(const OrderBook& /*book*/) noexcept {}
#endif

}  // namespace exchange
#pragma once

#include "command.hpp"
#include "types.hpp"

namespace exchange {

class OrderBook;  // forward declaration — avoids pulling in full order_book.hpp

// ── ValidationResult ──────────────────────────────────────────────────────────
//
// Tiny value type: 2 bytes, returned by value.
// ok == true  → command is valid, proceed to matching core.
// ok == false → emit ORDER_REJECTED with reason; do not create Order object.
//
// All validate() overloads are noexcept: validation never allocates and must
// not throw; it is on the critical path for every incoming command.
struct ValidationResult {
    bool         ok{true};
    RejectReason reason{RejectReason::None};

    [[nodiscard]] static ValidationResult accept() noexcept {
        return {true, RejectReason::None};
    }
    [[nodiscard]] static ValidationResult reject(RejectReason r) noexcept {
        return {false, r};
    }
};

static_assert(sizeof(ValidationResult) == 2, "ValidationResult must stay tiny");

// ── Validate overloads ────────────────────────────────────────────────────────
//
// Each overload checks syntactic and basic semantic rules for its command type.
// The matching core must not be called if validation returns !ok.
//
// Checks performed per command type (see Part 1, Section 10 for full spec):
//   NewLimitOrder  : quantity > 0, price > 0, valid side, no duplicate id
//   NewMarketOrder : quantity > 0, valid side, no duplicate id
//   Cancel         : target order exists and is active
//   Modify         : target exists, is active, new_quantity > 0, new_price > 0

[[nodiscard]] ValidationResult validate(
    const NewLimitOrderCmd&  cmd, const OrderBook& book) noexcept;

[[nodiscard]] ValidationResult validate(
    const NewMarketOrderCmd& cmd, const OrderBook& book) noexcept;

[[nodiscard]] ValidationResult validate(
    const CancelOrderCmd&    cmd, const OrderBook& book) noexcept;

[[nodiscard]] ValidationResult validate(
    const ModifyOrderCmd&    cmd, const OrderBook& book) noexcept;

}  // namespace exchange
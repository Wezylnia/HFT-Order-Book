#include "validation.hpp"

#include "order_book.hpp"

#include <cstdint>

namespace exchange {

namespace {

[[nodiscard]] bool side_is_valid(Side side) noexcept {
    const auto u = static_cast<std::uint8_t>(side);
    return u <= static_cast<std::uint8_t>(Side::Sell);
}

}  // namespace

// ── NewLimitOrderCmd ──────────────────────────────────────────────────────────
ValidationResult validate(const NewLimitOrderCmd& cmd,
                          const OrderBook& book) noexcept {
    if (cmd.quantity == 0)
        return ValidationResult::reject(RejectReason::InvalidQuantity);
    if (cmd.price == 0)
        return ValidationResult::reject(RejectReason::InvalidPrice);
    if (!side_is_valid(cmd.side))
        return ValidationResult::reject(RejectReason::InvalidSide);
    if (book.has_order(cmd.order_id))
        return ValidationResult::reject(RejectReason::DuplicateOrderId);
    return ValidationResult::accept();
}

// ── NewMarketOrderCmd ─────────────────────────────────────────────────────────
ValidationResult validate(const NewMarketOrderCmd& cmd,
                          const OrderBook& book) noexcept {
    if (cmd.quantity == 0)
        return ValidationResult::reject(RejectReason::InvalidQuantity);
    if (!side_is_valid(cmd.side))
        return ValidationResult::reject(RejectReason::InvalidSide);
    if (book.has_order(cmd.order_id))
        return ValidationResult::reject(RejectReason::DuplicateOrderId);
    return ValidationResult::accept();
}

// ── CancelOrderCmd ────────────────────────────────────────────────────────────
ValidationResult validate(const CancelOrderCmd& cmd,
                          const OrderBook& book) noexcept {
    Order* o = book.find(cmd.target_order_id);
    if (o == nullptr)
        return ValidationResult::reject(RejectReason::OrderNotFound);
    if (!o->is_active())
        return ValidationResult::reject(RejectReason::OrderNotActive);
    return ValidationResult::accept();
}

// ── ModifyOrderCmd ────────────────────────────────────────────────────────────
ValidationResult validate(const ModifyOrderCmd& cmd,
                          const OrderBook& book) noexcept {
    Order* o = book.find(cmd.target_order_id);
    if (o == nullptr)
        return ValidationResult::reject(RejectReason::OrderNotFound);
    if (!o->is_active())
        return ValidationResult::reject(RejectReason::OrderNotActive);
    if (cmd.new_quantity == 0)
        return ValidationResult::reject(RejectReason::InvalidQuantity);
    if (cmd.new_price == 0)
        return ValidationResult::reject(RejectReason::InvalidPrice);
    return ValidationResult::accept();
}

}  // namespace exchange

#pragma once

#include <cstdint>
#include <limits>

namespace exchange {

// ── Fundamental type aliases ──────────────────────────────────────────────────
using OrderId  = std::uint64_t;
using Price    = std::uint64_t;  // integer tick units (e.g. 1 tick = 0.01 → 10000 = 100.00)
using Quantity = std::uint64_t;
using SeqNo    = std::uint64_t;

inline constexpr OrderId  kInvalidOrderId = std::numeric_limits<OrderId>::max();
inline constexpr Price    kInvalidPrice   = 0;
inline constexpr Quantity kZeroQty        = 0;

// ── Side ──────────────────────────────────────────────────────────────────────
enum class Side : std::uint8_t {
    Buy  = 0,
    Sell = 1,
};

// ── Order type ────────────────────────────────────────────────────────────────
enum class OrderType : std::uint8_t {
    Limit  = 0,
    Market = 1,
};

// ── Order lifecycle state ─────────────────────────────────────────────────────
// NOTE: lifecycle starts at Accepted. Requests that fail validation never
// produce an Order object; they emit an ORDER_REJECTED event only.
enum class OrderState : std::uint8_t {
    Accepted        = 0,
    Active          = 1,
    PartiallyFilled = 2,
    Filled          = 3,
    Cancelled       = 4,
};

// ── Event types ───────────────────────────────────────────────────────────────
enum class EventType : std::uint8_t {
    OrderAccepted        = 0,
    OrderRested          = 1,
    OrderPartiallyFilled = 2,
    OrderFilled          = 3,
    OrderCancelled       = 4,
    OrderModified        = 5,
    OrderRejected        = 6,
    TradeExecuted        = 7,
};

// ── Reject reasons ────────────────────────────────────────────────────────────
enum class RejectReason : std::uint8_t {
    None             = 0,
    InvalidQuantity  = 1,
    InvalidPrice     = 2,
    InvalidSide      = 3,
    DuplicateOrderId = 4,
    OrderNotFound    = 5,
    OrderNotActive   = 6,
    InvalidModify    = 7,
};

// ── Time-in-Force ─────────────────────────────────────────────────────────────
enum class TimeInForce : std::uint8_t {
    GTC = 0,   // Good-Till-Cancelled — residual rests on book (default)
    IOC = 1,   // Immediate-Or-Cancel — residual discarded after match attempt
    FOK = 2,   // Fill-Or-Kill        — fully fill or immediately cancel (no trade)
};

}  // namespace exchange
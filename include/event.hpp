#pragma once

#include "types.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace exchange {

// ── TradeEvent — 48 bytes ─────────────────────────────────────────────────────
//
// Emitted once per maker–taker match. Trade price is always the resting
// (maker) order's price — standard CLOB convention.
//
// Layout: all 8-byte fields first, Side (1B) last with explicit padding.
struct TradeEvent {
    SeqNo    seq_no{0};                         // [0 ]
    OrderId  buy_order_id{0};                   // [8 ]
    OrderId  sell_order_id{0};                  // [16]
    Price    trade_price{0};                    // [24]
    Quantity trade_qty{0};                      // [32]
    Side     aggressor_side{Side::Buy};         // [40] the incoming (taker) side
    uint8_t  pad_[7]{};                         // [41] explicit padding → sizeof == 48
};

static_assert(sizeof(TradeEvent) == 48, "TradeEvent layout changed unexpectedly");

// ── OrderEvent — 32 bytes ─────────────────────────────────────────────────────
//
// Covers all order lifecycle transitions:
//   Accepted, Rested, PartiallyFilled, Filled, Cancelled, Modified, Rejected.
//
// For REJECTED events: reject_reason carries the cause; order_id is the
// request's order_id (no Order object was created).
// For ACCEPTED events: remaining_qty == original requested quantity.
// For all other events: remaining_qty is the current remaining quantity.
//
// Original layout had 7-byte padding after `type` (uint8_t) before `order_id`.
// Reordering all 8-byte fields to the front eliminates that gap:
struct OrderEvent {
    SeqNo        seq_no{0};                            // [0 ]
    OrderId      order_id{0};                          // [8 ]
    Quantity     remaining_qty{0};                     // [16]
    EventType    type{EventType::OrderAccepted};       // [24]
    RejectReason reject_reason{RejectReason::None};    // [25] meaningful only for Rejected
    uint8_t      pad_[6]{};                            // [26] explicit padding → sizeof == 32
};

static_assert(sizeof(OrderEvent) == 32, "OrderEvent layout changed unexpectedly");

// ── Unified event ─────────────────────────────────────────────────────────────
//
// std::variant discriminant adds ~8 bytes alignment overhead.
// sizeof(Event) == sizeof(TradeEvent) + alignment == ~56 bytes.
// For ultra-low-latency future work: replace with tagged union or separate queues.
using Event     = std::variant<TradeEvent, OrderEvent>;
using EventList = std::vector<Event>;

// ── Factory helpers ───────────────────────────────────────────────────────────
//
// Inline factories keep event construction out of engine.cpp and make intent
// explicit at the call site. [[nodiscard]] prevents accidentally ignoring them.

[[nodiscard]] inline TradeEvent make_trade(
    SeqNo seq, OrderId buy_id, OrderId sell_id,
    Price price, Quantity qty, Side aggressor) noexcept
{
    return {seq, buy_id, sell_id, price, qty, aggressor};
}

[[nodiscard]] inline OrderEvent make_order_event(
    SeqNo seq, OrderId id, EventType type, Quantity remaining,
    RejectReason reason = RejectReason::None) noexcept
{
    return {seq, id, remaining, type, reason};
}

}  // namespace exchange
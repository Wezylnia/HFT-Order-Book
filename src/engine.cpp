#include "engine.hpp"

#include "event.hpp"
#include "order.hpp"
#include "order_book.hpp"
#include "validation.hpp"

#include <algorithm>
#include <cassert>
#include <memory>

namespace exchange {

// ── File-local helpers ────────────────────────────────────────────────────────

// Returns true if incoming's price is willing to trade against `best`.
// Market orders (price == kInvalidPrice == 0) always cross any resting level.
// Limit Buy  : bid must be >= best ask  (I pay at least what they're asking)
// Limit Sell : ask must be <= best bid  (I accept at most what they're bidding)
static bool price_crosses(const Order* incoming, const PriceLevel* best) noexcept {
    if (incoming->price == kInvalidPrice) return true;
    return (incoming->side == Side::Buy) ? (incoming->price >= best->price)
                                         : (incoming->price <= best->price);
}

// ── submit — New Limit Order (Milestone 2) ────────────────────────────────────

EventList MatchingEngine::submit(const NewLimitOrderCmd& cmd) {
    EventList out;
    out.reserve(kEventReserve);

    // ── 1. Validation ─────────────────────────────────────────────────────────
    // Syntactic + basic semantic checks (qty, price, duplicate id).
    // If invalid: emit a single Rejected event and return immediately.
    // No Order object is created on rejection — the lifecycle never starts.
    const ValidationResult vr = validate(cmd, book_);
    if (!vr.ok) {
        out.push_back(make_order_event(
            advance_seq(), cmd.order_id, EventType::OrderRejected, 0, vr.reason));
        return out;
    }

    // ── 2. Order creation ─────────────────────────────────────────────────────
    // Allocates an Order, transfers ownership into book_.order_index_, and
    // returns a stable raw pointer for the rest of this call.
    // The seq_no is shared between the Order object and the Accepted event so
    // that replayers can tie the event back to the physical order.
    const SeqNo seq = advance_seq();
    Order* incoming = make_limit_order(cmd, seq);

    out.push_back(make_order_event(
        seq, incoming->id, EventType::OrderAccepted, incoming->remaining_qty));

    // ── 3. Matching ───────────────────────────────────────────────────────────
    // Sweeps the opposite side FIFO until the incoming is exhausted or no
    // more price-crossing levels remain.  All trade and fill events are
    // appended to `out` inside match().
    match(incoming, out);

    // ── 4. Residual resting vs full fill ───────────────────────────────────────
    // Fully filled incoming limits are unregistered here — they never rested, so
    // the ownership map must not retain dead Order objects.
    if (incoming->remaining_qty > 0) {
        incoming->state = OrderState::Active;
        book_.place_on_book(incoming);
        out.push_back(make_order_event(
            advance_seq(), incoming->id, EventType::OrderRested, incoming->remaining_qty));
    } else {
        auto discard = book_.unregister_order(incoming->id);
    }

    return out;
}

// ── submit — New Market Order ───────────────────────────────────────────────────
//
// Market orders never rest on the book (price == 0 violates place_on_book).
// Residual quantity after matching is discarded and emits OrderCancelled.
// Ownership of the transient Order is always released at the end of this call.

EventList MatchingEngine::submit(const NewMarketOrderCmd& cmd) {
    EventList out;
    out.reserve(kEventReserve);

    const ValidationResult vr = validate(cmd, book_);
    if (!vr.ok) {
        out.push_back(make_order_event(
            advance_seq(), cmd.order_id, EventType::OrderRejected, 0, vr.reason));
        return out;
    }

    const SeqNo seq = advance_seq();
    Order* incoming = make_market_order(cmd, seq);

    out.push_back(make_order_event(
        seq, incoming->id, EventType::OrderAccepted, incoming->remaining_qty));

    match(incoming, out);

    if (incoming->remaining_qty > 0) {
        out.push_back(make_order_event(
            advance_seq(), incoming->id, EventType::OrderCancelled,
            incoming->remaining_qty));
    }

    auto owner = book_.unregister_order(incoming->id);
    return out;
}

// ── submit — Cancel (Milestone 3 stub) ───────────────────────────────────────

EventList MatchingEngine::submit(const CancelOrderCmd& /*cmd*/) {
    EventList out;
    out.reserve(kEventReserve);
    // TODO M3: validate → remove_from_book → unregister_order → emit Cancelled
    return out;
}

// ── submit — Modify (Milestone 3 stub) ───────────────────────────────────────

EventList MatchingEngine::submit(const ModifyOrderCmd& /*cmd*/) {
    EventList out;
    out.reserve(kEventReserve);
    // TODO M3: cancel + re-submit at new price/qty (time priority resets)
    return out;
}

// ── reset ─────────────────────────────────────────────────────────────────────

void MatchingEngine::reset() {
    book_     = OrderBook{};   // constructs a fresh book; old one is destroyed
    next_seq_ = 0;
}

// ── match — Core match loop ───────────────────────────────────────────────────
//
// Design contract:
//   • Caller has already validated and accepted `incoming`.
//   • `incoming` is NOT yet on the book when this is called.
//   • This function only emits events for trades and maker lifecycle changes.
//   • Incoming's final state (Rested or Filled) is handled by the caller.
//
// Iteration strategy — two nested loops:
//   Outer: advance to the next best level on the opposite side.
//   Inner: consume orders FIFO within the current level.
//
// Key invariant: after each inner-loop iteration, either
//   (a) incoming->remaining_qty == 0  →  outer loop exits, or
//   (b) the current maker was fully filled and removed, so the inner loop
//       re-fetches best->front() for the next maker, or
//   (c) the level was fully drained and erased, so the outer loop moves on.
void MatchingEngine::match(Order* incoming, EventList& out) {
    BookSide& opposite = (incoming->side == Side::Buy) ? book_.asks()
                                                       : book_.bids();

    while (incoming->remaining_qty > 0) {

        // ── Outer: get best resting level ─────────────────────────────────────
        PriceLevel* best = opposite.best_level();
        if (best == nullptr || !price_crosses(incoming, best)) break;

        // ── Inner: drain this level FIFO ──────────────────────────────────────
        while (incoming->remaining_qty > 0 && !best->empty()) {
            Order* maker = best->front();
            assert(maker != nullptr);
            assert(maker->level == best);

            // Trade quantity: the smaller of the two sides
            const Quantity trade_qty =
                std::min(incoming->remaining_qty, maker->remaining_qty);

            // TradeEvent: buy_id / sell_id always in canonical (buy, sell) order
            const OrderId buy_id  = (incoming->side == Side::Buy) ? incoming->id
                                                                   : maker->id;
            const OrderId sell_id = (incoming->side == Side::Buy) ? maker->id
                                                                   : incoming->id;

            // Trade price = maker's price (standard CLOB convention:
            // the resting order dictates the execution price).
            out.push_back(make_trade(
                advance_seq(), buy_id, sell_id,
                maker->price, trade_qty, incoming->side));

            // ── Quantity accounting ───────────────────────────────────────────
            // We decrement level.total_qty here so PriceLevel::remove() sees
            // maker->remaining_qty == 0 on a fully-filled maker and skips
            // its own total_qty adjustment (avoiding a double-deduct).
            incoming->remaining_qty -= trade_qty;
            maker->remaining_qty    -= trade_qty;
            best->total_qty         -= trade_qty;

            // ── Maker resolution ──────────────────────────────────────────────
            if (maker->is_fully_filled()) {
                maker->state = OrderState::Filled;
                out.push_back(make_order_event(
                    advance_seq(), maker->id, EventType::OrderFilled, 0));

                // Remove from intrusive list (remaining_qty == 0 → no total_qty change)
                best->remove(maker);

                // Release ownership: unique_ptr goes out of scope at end of block → Order destroyed.
                // `maker` is dangling after this line — do not use it again.
                auto owner = book_.unregister_order(maker->id);

                if (best->empty()) {
                    // Capture price before the pointer becomes dangling
                    const Price drained_price = best->price;
                    opposite.erase_level(drained_price);
                    break;  // exit inner loop; outer loop will fetch the new best
                }
                // else: more orders remain in this level → inner loop continues

            } else {
                // Maker partially filled: incoming exhausted this maker first.
                // incoming->remaining_qty == 0 here, so the inner while exits
                // on the next condition check — no explicit break needed.
                maker->state = OrderState::PartiallyFilled;
                out.push_back(make_order_event(
                    advance_seq(), maker->id,
                    EventType::OrderPartiallyFilled, maker->remaining_qty));
            }
        }
    }

    // ── Incoming final state ──────────────────────────────────────────────────
    // Emit OrderFilled only if fully consumed in the match loop.
    // Partial fill with residual: the caller (submit) will emit OrderRested.
    if (incoming->is_fully_filled()) {
        incoming->state = OrderState::Filled;
        out.push_back(make_order_event(
            advance_seq(), incoming->id, EventType::OrderFilled, 0));
    }
}

// ── make_limit_order ──────────────────────────────────────────────────────────
//
// Heap-allocates an Order from command fields, registers it in the book's
// ownership map, and returns a stable raw pointer.
// The same seq_no is stored on the Order (for replay) and used in the
// OrderAccepted event so the two can always be correlated.
Order* MatchingEngine::make_limit_order(const NewLimitOrderCmd& cmd, SeqNo seq) {
    auto o           = std::make_unique<Order>();
    o->id            = cmd.order_id;
    o->remaining_qty = cmd.quantity;
    o->price         = cmd.price;
    o->side          = cmd.side;
    o->seq_no        = seq;
    o->state         = OrderState::Accepted;
    // prev, next, level default to nullptr — order is not yet on the book
    return book_.register_order(std::move(o));
}

// ── make_market_order ─────────────────────────────────────────────────────────
//
// Same ownership path as make_limit_order, but price = kInvalidPrice so that
// price_crosses() treats every opposite level as tradeable.
Order* MatchingEngine::make_market_order(const NewMarketOrderCmd& cmd, SeqNo seq) {
    auto o           = std::make_unique<Order>();
    o->id            = cmd.order_id;
    o->remaining_qty = cmd.quantity;
    o->price         = kInvalidPrice;
    o->side          = cmd.side;
    o->seq_no        = seq;
    o->state         = OrderState::Accepted;
    return book_.register_order(std::move(o));
}

}  // namespace exchange
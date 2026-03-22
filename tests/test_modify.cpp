// Milestone 3 — modify order unit tests
#include <gtest/gtest.h>

#include "command.hpp"
#include "engine.hpp"
#include "event.hpp"
#include "order_book.hpp"
#include "types.hpp"

#include <variant>
#include <vector>

namespace exchange {
namespace {

[[nodiscard]] std::vector<const TradeEvent*> collect_trades(const EventList& evs) {
    std::vector<const TradeEvent*> out;
    for (const auto& e : evs)
        if (const auto* t = std::get_if<TradeEvent>(&e))
            out.push_back(t);
    return out;
}

[[nodiscard]] std::size_t count_order_events(const EventList& evs, EventType t) {
    std::size_t n = 0;
    for (const auto& e : evs)
        if (const auto* oe = std::get_if<OrderEvent>(&e))
            if (oe->type == t) ++n;
    return n;
}

[[nodiscard]] const OrderEvent* first_order_event(const EventList& evs, EventType t) {
    for (const auto& e : evs)
        if (const auto* oe = std::get_if<OrderEvent>(&e))
            if (oe->type == t) return oe;
    return nullptr;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

// Modify resets FIFO time priority: modified order goes to the back of the queue.
// Setup: S1 then S2 at same price. After modifying S1, a BUY should match S2 first.
TEST(Modify, PriorityReset) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Sell, .price = 100, .quantity = 5});

    // Modify S1: same price, same qty — only effect is priority reset to tail
    (void)eng.submit(ModifyOrderCmd{
        .target_order_id = 1, .new_price = 100, .new_quantity = 5});

    // Queue is now: S2 (head) → S1 (tail)
    // BUY qty=5 should exhaust S2 first
    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 3, .side = Side::Buy, .price = 100, .quantity = 5});

    const auto trades = collect_trades(ev);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0]->sell_order_id, 2u);   // S2 matched, not S1

    // S2 gone, S1 still on book
    EXPECT_FALSE(eng.book().has_order(2));
    ASSERT_TRUE(eng.book().has_order(1));
    EXPECT_EQ(eng.book().find(1)->remaining_qty, 5u);
}

// Modifying price such that it crosses the opposite side triggers immediate matching
TEST(Modify, PriceCross) {
    MatchingEngine eng;

    // Resting ask @100
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});
    // Resting bid @99 — no cross
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 99, .quantity = 5});

    // Raise bid to 100 → now crosses the ask
    const EventList ev = eng.submit(ModifyOrderCmd{
        .target_order_id = 2, .new_price = 100, .new_quantity = 5});

    const auto trades = collect_trades(ev);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0]->trade_price, 100u);   // maker price (ask) dictates trade price
    EXPECT_EQ(trades[0]->trade_qty, 5u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderModified), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 2u);  // both sides

    EXPECT_FALSE(eng.book().has_order(1));
    EXPECT_FALSE(eng.book().has_order(2));
}

// Modifying a non-existent order → ORDER_REJECTED(OrderNotFound)
TEST(Modify, NonExistent) {
    MatchingEngine eng;

    const EventList ev = eng.submit(ModifyOrderCmd{
        .target_order_id = 999, .new_price = 100, .new_quantity = 5});

    const OrderEvent* rej = first_order_event(ev, EventType::OrderRejected);
    ASSERT_NE(rej, nullptr);
    EXPECT_EQ(rej->order_id, 999u);
    EXPECT_EQ(rej->reject_reason, RejectReason::OrderNotFound);
}

// Reducing quantity via modify: book must reflect the new (smaller) qty
TEST(Modify, QuantityDown) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});

    const EventList ev = eng.submit(ModifyOrderCmd{
        .target_order_id = 1, .new_price = 100, .new_quantity = 2});

    const OrderEvent* mod = first_order_event(ev, EventType::OrderModified);
    ASSERT_NE(mod, nullptr);
    EXPECT_EQ(mod->order_id, 1u);
    EXPECT_EQ(mod->remaining_qty, 2u);

    // Book must reflect the reduced quantity
    ASSERT_TRUE(eng.book().has_order(1));
    const Order* o = eng.book().find(1);
    EXPECT_EQ(o->remaining_qty, 2u);
    EXPECT_EQ(o->price, 100u);

    // Level volume must also be updated
    const PriceLevel* lvl = eng.book().asks().find_level(100);
    ASSERT_NE(lvl, nullptr);
    EXPECT_EQ(lvl->total_qty, 2u);
}

// Modifying with new_quantity == 0 → ORDER_REJECTED(InvalidQuantity)
TEST(Modify, ZeroQuantityRejected) {
    MatchingEngine eng;
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});

    const EventList ev = eng.submit(ModifyOrderCmd{
        .target_order_id = 1, .new_price = 100, .new_quantity = 0});

    const OrderEvent* rej = first_order_event(ev, EventType::OrderRejected);
    ASSERT_NE(rej, nullptr);
    EXPECT_EQ(rej->reject_reason, RejectReason::InvalidQuantity);

    // Original order untouched
    ASSERT_TRUE(eng.book().has_order(1));
    EXPECT_EQ(eng.book().find(1)->remaining_qty, 10u);
}

}  // namespace exchange

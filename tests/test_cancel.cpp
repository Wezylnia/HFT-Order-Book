// Milestone 3 — cancel order unit tests
#include <gtest/gtest.h>

#include "command.hpp"
#include "engine.hpp"
#include "event.hpp"
#include "order_book.hpp"
#include "types.hpp"

#include <variant>

namespace exchange {
namespace {

[[nodiscard]] const OrderEvent* first_order_event(const EventList& evs, EventType t) {
    for (const auto& e : evs)
        if (const auto* oe = std::get_if<OrderEvent>(&e))
            if (oe->type == t) return oe;
    return nullptr;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

// Cancel a resting limit order: must emit OrderCancelled and remove from book
TEST(Cancel, ActiveOrder) {
    MatchingEngine eng;
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    ASSERT_TRUE(eng.book().has_order(1));

    const EventList ev = eng.submit(CancelOrderCmd{.target_order_id = 1});

    const OrderEvent* cancelled = first_order_event(ev, EventType::OrderCancelled);
    ASSERT_NE(cancelled, nullptr);
    EXPECT_EQ(cancelled->order_id, 1u);
    EXPECT_EQ(cancelled->remaining_qty, 10u);

    EXPECT_FALSE(eng.book().has_order(1));
    EXPECT_TRUE(eng.book().bids().empty());
}

// Cancelling a partially-filled resting order carries the current remaining_qty
TEST(Cancel, PartiallyFilledOrder) {
    MatchingEngine eng;

    // Sell 10, buy 3 → sell has 7 remaining and is still on the book
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 3});

    ASSERT_TRUE(eng.book().has_order(1));
    EXPECT_EQ(eng.book().find(1)->remaining_qty, 7u);

    const EventList ev = eng.submit(CancelOrderCmd{.target_order_id = 1});

    const OrderEvent* cancelled = first_order_event(ev, EventType::OrderCancelled);
    ASSERT_NE(cancelled, nullptr);
    EXPECT_EQ(cancelled->remaining_qty, 7u);

    EXPECT_FALSE(eng.book().has_order(1));
    EXPECT_TRUE(eng.book().asks().empty());
}

// Cancelling a non-existent order_id → ORDER_REJECTED(OrderNotFound)
TEST(Cancel, NonExistent) {
    MatchingEngine eng;

    const EventList ev = eng.submit(CancelOrderCmd{.target_order_id = 999});

    const OrderEvent* rej = first_order_event(ev, EventType::OrderRejected);
    ASSERT_NE(rej, nullptr);
    EXPECT_EQ(rej->order_id, 999u);
    EXPECT_EQ(rej->reject_reason, RejectReason::OrderNotFound);
}

// A filled order is immediately unregistered — cancelling its id gives OrderNotFound
// (not OrderNotActive, because there's no longer a book entry to inspect)
TEST(Cancel, FilledOrder_GivesNotFound) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 5});

    // Order 1 is now fully filled and unregistered
    ASSERT_FALSE(eng.book().has_order(1));

    const EventList ev = eng.submit(CancelOrderCmd{.target_order_id = 1});

    const OrderEvent* rej = first_order_event(ev, EventType::OrderRejected);
    ASSERT_NE(rej, nullptr);
    EXPECT_EQ(rej->reject_reason, RejectReason::OrderNotFound);
}

// Cancelling the sole order at a price level must erase that level from the book
TEST(Cancel, CleansEmptyLevel) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Buy, .price = 95, .quantity = 5});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 5});

    // Two bid levels: 95 and 100
    ASSERT_EQ(eng.book().bids().levels().size(), 2u);

    (void)eng.submit(CancelOrderCmd{.target_order_id = 2});

    // Level @100 is gone; level @95 still exists
    ASSERT_EQ(eng.book().bids().levels().size(), 1u);
    EXPECT_EQ(eng.book().bids().best_level()->price, 95u);
}

}  // namespace exchange

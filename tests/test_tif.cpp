// Milestone 3-Ek — TimeInForce (GTC / IOC / FOK) unit tests
#include <gtest/gtest.h>

#include "command.hpp"
#include "engine.hpp"
#include "event.hpp"
#include "types.hpp"

#include <variant>
#include <vector>

using namespace exchange;

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

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
// GTC (default) — existing behavior must be preserved
// ═════════════════════════════════════════════════════════════════════════════

// GTC with no cross: residual rests on book
TEST(TIF_GTC, ResidualRests) {
    MatchingEngine eng;
    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::GTC});

    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 1u);
    EXPECT_TRUE(eng.book().has_order(1));
}

// GTC with partial cross: unfilled residual stays on book
TEST(TIF_GTC, PartialCross_RestsRemainder) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::GTC});

    ASSERT_EQ(collect_trades(ev).size(), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 1u);
    ASSERT_TRUE(eng.book().has_order(2));
    EXPECT_EQ(eng.book().find(2)->remaining_qty, 5u);
}

// ═════════════════════════════════════════════════════════════════════════════
// IOC — residual discarded after match, never rests on book
// ═════════════════════════════════════════════════════════════════════════════

// IOC: full fill — no residual, no OrderCancelled
TEST(TIF_IOC, FullFill_NoCancelled) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::IOC});

    ASSERT_EQ(collect_trades(ev).size(), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 2u);   // maker + taker
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 0u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 0u);
    EXPECT_FALSE(eng.book().has_order(2));
}

// IOC: partial fill — matched portion trades, residual discarded → OrderCancelled
TEST(TIF_IOC, PartialFill_ResidualDiscarded) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::IOC});

    // trade happened for 5 units
    ASSERT_EQ(collect_trades(ev).size(), 1u);
    EXPECT_EQ(collect_trades(ev)[0]->trade_qty, 5u);

    // residual 5 units discarded
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 0u);

    // order must not be on the book
    EXPECT_FALSE(eng.book().has_order(2));
    EXPECT_TRUE(eng.book().asks().empty());
}

// IOC: empty book — accepted then immediately cancelled, no trades
TEST(TIF_IOC, EmptyBook_ImmediatelyCancelled) {
    MatchingEngine eng;

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::IOC});

    EXPECT_TRUE(collect_trades(ev).empty());
    EXPECT_EQ(count_order_events(ev, EventType::OrderAccepted), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 0u);
    EXPECT_FALSE(eng.book().has_order(1));
}

// IOC: never rests on book regardless of price
TEST(TIF_IOC, NeverRestsOnBook) {
    MatchingEngine eng;

    // submit IOC buy at 100: no matching ask → immediately cancelled
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::IOC});

    EXPECT_TRUE(eng.book().bids().empty());
    EXPECT_FALSE(eng.book().has_order(1));
}

// ═════════════════════════════════════════════════════════════════════════════
// FOK — fill fully or cancel entirely (no partial execution)
// ═════════════════════════════════════════════════════════════════════════════

// FOK: exact liquidity available — fully filled
TEST(TIF_FOK, ExactLiquidity_FullFill) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::FOK});

    ASSERT_EQ(collect_trades(ev).size(), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 2u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 0u);
    EXPECT_FALSE(eng.book().has_order(2));
}

// FOK: more liquidity than needed — fully filled from single level
TEST(TIF_FOK, SurplusLiquidity_FullFill) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 20});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::FOK});

    ASSERT_EQ(collect_trades(ev).size(), 1u);
    EXPECT_EQ(collect_trades(ev)[0]->trade_qty, 10u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 0u);
    EXPECT_FALSE(eng.book().has_order(2));
    // maker partially filled, still on book
    ASSERT_TRUE(eng.book().has_order(1));
    EXPECT_EQ(eng.book().find(1)->remaining_qty, 10u);
}

// FOK: insufficient liquidity — zero trades, order cancelled
TEST(TIF_FOK, InsufficientLiquidity_NoCross) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::FOK});

    // no trades must occur
    EXPECT_TRUE(collect_trades(ev).empty());
    EXPECT_EQ(count_order_events(ev, EventType::OrderAccepted), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 1u);

    // resting ask must be untouched
    EXPECT_FALSE(eng.book().has_order(2));
    ASSERT_TRUE(eng.book().has_order(1));
    EXPECT_EQ(eng.book().find(1)->remaining_qty, 5u);
}

// FOK: empty book — accepted then immediately cancelled, no trades
TEST(TIF_FOK, EmptyBook_ImmediatelyCancelled) {
    MatchingEngine eng;

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Buy, .price = 100, .quantity = 10,
        .tif = TimeInForce::FOK});

    EXPECT_TRUE(collect_trades(ev).empty());
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 1u);
    EXPECT_FALSE(eng.book().has_order(1));
}

// FOK: multi-level — sufficient aggregate liquidity across levels → fully fills
TEST(TIF_FOK, MultiLevel_SufficientAggregate_FullFill) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Sell, .price = 101, .quantity = 5});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 3, .side = Side::Buy, .price = 101, .quantity = 10,
        .tif = TimeInForce::FOK});

    ASSERT_EQ(collect_trades(ev).size(), 2u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 3u);  // 2 makers + taker
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 0u);
    EXPECT_FALSE(eng.book().has_order(3));
}

// FOK: multi-level — insufficient aggregate across levels → nothing executes
TEST(TIF_FOK, MultiLevel_InsufficientAggregate_NoCross) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 3});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Sell, .price = 101, .quantity = 3});

    // want 10, only 6 available
    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 3, .side = Side::Buy, .price = 101, .quantity = 10,
        .tif = TimeInForce::FOK});

    EXPECT_TRUE(collect_trades(ev).empty());
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 1u);

    // resting orders must be untouched
    ASSERT_TRUE(eng.book().has_order(1));
    ASSERT_TRUE(eng.book().has_order(2));
    EXPECT_EQ(eng.book().find(1)->remaining_qty, 3u);
    EXPECT_EQ(eng.book().find(2)->remaining_qty, 3u);
}

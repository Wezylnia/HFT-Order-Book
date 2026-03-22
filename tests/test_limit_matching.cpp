// Milestone 2 — limit / market matching unit tests
#include <gtest/gtest.h>

#include "command.hpp"
#include "engine.hpp"
#include "event.hpp"
#include "order_book.hpp"
#include "types.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace exchange {
namespace {

[[nodiscard]] std::vector<const TradeEvent*> collect_trades(const EventList& evs) {
    std::vector<const TradeEvent*> out;
    for (const auto& e : evs) {
        if (const auto* t = std::get_if<TradeEvent>(&e)) {
            out.push_back(t);
        }
    }
    return out;
}

[[nodiscard]] std::size_t count_order_events(const EventList& evs, EventType t) {
    std::size_t n = 0;
    for (const auto& e : evs) {
        if (const auto* oe = std::get_if<OrderEvent>(&e)) {
            if (oe->type == t) ++n;
        }
    }
    return n;
}

// Uncrossed book: when both sides have liquidity, best_bid must be strictly
// below best_ask. (Equality would still be a crossing price.)
void expect_no_cross(const OrderBook& book) {
    const PriceLevel* bid = book.bids().best_level();
    const PriceLevel* ask = book.asks().best_level();
    if (bid != nullptr && ask != nullptr) {
        EXPECT_LT(bid->price, ask->price)
            << "crossed book: best_bid=" << bid->price << " best_ask=" << ask->price;
    }
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

TEST(LimitMatching, EmptyBook_LimitRests) {
    MatchingEngine eng;
    const EventList ev = eng.submit(NewLimitOrderCmd{.order_id = 1,
                                                     .side     = Side::Buy,
                                                     .price    = 100,
                                                     .quantity = 10});

    EXPECT_EQ(count_order_events(ev, EventType::OrderAccepted), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 1u);
    EXPECT_TRUE(collect_trades(ev).empty());

    ASSERT_TRUE(eng.book().has_order(1));
    const Order* o = eng.book().find(1);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->price, 100u);
    EXPECT_EQ(o->remaining_qty, 10u);
    expect_no_cross(eng.book());
}

TEST(LimitMatching, SimpleCross_FullFill) {
    MatchingEngine eng;

    const EventList ev_sell = eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});
    EXPECT_EQ(count_order_events(ev_sell, EventType::OrderRested), 1u);
    expect_no_cross(eng.book());

    const EventList ev_buy = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10});

    const auto trades = collect_trades(ev_buy);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0]->trade_price, 100u);
    EXPECT_EQ(trades[0]->trade_qty, 10u);
    EXPECT_EQ(trades[0]->buy_order_id, 2u);
    EXPECT_EQ(trades[0]->sell_order_id, 1u);

    EXPECT_EQ(count_order_events(ev_buy, EventType::OrderFilled), 2u);  // maker + taker

    EXPECT_FALSE(eng.book().has_order(1));
    EXPECT_FALSE(eng.book().has_order(2));
    expect_no_cross(eng.book());
}

TEST(LimitMatching, SimpleCross_PartialMaker) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 50});

    const auto trades = collect_trades(ev);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0]->trade_qty, 10u);

    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 1u);   // maker
    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 1u);   // taker residual

    EXPECT_FALSE(eng.book().has_order(1));
    ASSERT_TRUE(eng.book().has_order(2));
    const Order* taker = eng.book().find(2);
    ASSERT_NE(taker, nullptr);
    EXPECT_EQ(taker->remaining_qty, 40u);
    EXPECT_EQ(taker->price, 100u);
    expect_no_cross(eng.book());
}

TEST(LimitMatching, SimpleCross_PartialTaker) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 50});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 10});

    const auto trades = collect_trades(ev);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0]->trade_qty, 10u);

    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 1u);  // taker only

    EXPECT_FALSE(eng.book().has_order(2));
    ASSERT_TRUE(eng.book().has_order(1));
    const Order* maker = eng.book().find(1);
    ASSERT_NE(maker, nullptr);
    EXPECT_EQ(maker->remaining_qty, 40u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderPartiallyFilled), 1u);
    expect_no_cross(eng.book());
}

TEST(LimitMatching, SamePriceFIFO) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Sell, .price = 100, .quantity = 7});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 3, .side = Side::Buy, .price = 100, .quantity = 12});

    const auto trades = collect_trades(ev);
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0]->sell_order_id, 1u);
    EXPECT_EQ(trades[0]->trade_qty, 5u);
    EXPECT_EQ(trades[1]->sell_order_id, 2u);
    EXPECT_EQ(trades[1]->trade_qty, 7u);

    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 3u);  // S1, S2, buyer

    EXPECT_FALSE(eng.book().has_order(1));
    EXPECT_FALSE(eng.book().has_order(2));
    EXPECT_FALSE(eng.book().has_order(3));
    expect_no_cross(eng.book());
}

TEST(LimitMatching, MultiLevelSweep) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 101, .quantity = 1});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Sell, .price = 102, .quantity = 1});
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 3, .side = Side::Sell, .price = 103, .quantity = 1});

    const EventList ev = eng.submit(NewMarketOrderCmd{
        .order_id = 4, .side = Side::Buy, .quantity = 3});

    const auto trades = collect_trades(ev);
    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0]->trade_price, 101u);
    EXPECT_EQ(trades[1]->trade_price, 102u);
    EXPECT_EQ(trades[2]->trade_price, 103u);

    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 4u);  // 3 makers + taker

    EXPECT_FALSE(eng.book().has_order(4));
    expect_no_cross(eng.book());
}

TEST(LimitMatching, ResidualResting) {
    MatchingEngine eng;

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});

    const EventList ev = eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Buy, .price = 100, .quantity = 25});

    ASSERT_EQ(collect_trades(ev).size(), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderRested), 1u);

    const Order* o = eng.book().find(2);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->remaining_qty, 15u);
    EXPECT_EQ(o->price, 100u);
    expect_no_cross(eng.book());
}

TEST(LimitMatching, NoCrossAfterMatch) {
    MatchingEngine eng;

    auto step = [&eng]() { expect_no_cross(eng.book()); };

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Buy, .price = 99, .quantity = 5});
    step();

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 2, .side = Side::Sell, .price = 101, .quantity = 5});
    step();

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 3, .side = Side::Buy, .price = 100, .quantity = 2});
    step();

    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 4, .side = Side::Sell, .price = 100, .quantity = 1});
    step();
}

}  // namespace exchange

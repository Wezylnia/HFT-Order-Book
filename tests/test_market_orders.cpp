// Milestone 3 — market order unit tests
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

// Resting liquidity exactly matches market order quantity → fully filled
TEST(MarketOrder, FullFill) {
    MatchingEngine eng;
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 10});

    const EventList ev = eng.submit(NewMarketOrderCmd{
        .order_id = 2, .side = Side::Buy, .quantity = 10});

    const auto trades = collect_trades(ev);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0]->trade_qty, 10u);
    EXPECT_EQ(trades[0]->trade_price, 100u);

    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 2u);  // maker + taker
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 0u);

    EXPECT_FALSE(eng.book().has_order(1));
    EXPECT_FALSE(eng.book().has_order(2));
}

// Liquidity is less than market order quantity → partial fill, residual discarded
TEST(MarketOrder, PartialFill_Discard) {
    MatchingEngine eng;
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 5});

    const EventList ev = eng.submit(NewMarketOrderCmd{
        .order_id = 2, .side = Side::Buy, .quantity = 10});

    ASSERT_EQ(collect_trades(ev).size(), 1u);
    EXPECT_EQ(collect_trades(ev)[0]->trade_qty, 5u);

    EXPECT_EQ(count_order_events(ev, EventType::OrderFilled), 1u);    // maker only
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 1u); // taker residual

    const OrderEvent* cancel = first_order_event(ev, EventType::OrderCancelled);
    ASSERT_NE(cancel, nullptr);
    EXPECT_EQ(cancel->order_id, 2u);
    EXPECT_EQ(cancel->remaining_qty, 5u);   // 10 - 5 = 5 discarded

    EXPECT_FALSE(eng.book().has_order(1));
    EXPECT_FALSE(eng.book().has_order(2));
}

// No liquidity on opposite side → accepted then immediately cancelled
TEST(MarketOrder, EmptyBook) {
    MatchingEngine eng;

    const EventList ev = eng.submit(NewMarketOrderCmd{
        .order_id = 1, .side = Side::Buy, .quantity = 5});

    EXPECT_TRUE(collect_trades(ev).empty());
    EXPECT_EQ(count_order_events(ev, EventType::OrderAccepted), 1u);
    EXPECT_EQ(count_order_events(ev, EventType::OrderCancelled), 1u);

    const OrderEvent* cancel = first_order_event(ev, EventType::OrderCancelled);
    ASSERT_NE(cancel, nullptr);
    EXPECT_EQ(cancel->remaining_qty, 5u);

    EXPECT_FALSE(eng.book().has_order(1));
}

// After any market order, no bid remains in the book
TEST(MarketOrder, NeverRests) {
    MatchingEngine eng;
    (void)eng.submit(NewLimitOrderCmd{
        .order_id = 1, .side = Side::Sell, .price = 100, .quantity = 3});

    (void)eng.submit(NewMarketOrderCmd{
        .order_id = 2, .side = Side::Buy, .quantity = 10});

    EXPECT_TRUE(eng.book().bids().empty());
    EXPECT_FALSE(eng.book().has_order(2));
}

}  // namespace exchange

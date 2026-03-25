// Milestone 1 — PriceLevel + BookSide + OrderBook unit tests
#include <gtest/gtest.h>

#include "order.hpp"
#include "order_book.hpp"
#include "price_level.hpp"
#include "types.hpp"

#include <memory>
#include <vector>

using namespace exchange;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers: allocate heap Orders that won't be managed by OrderBook
// ─────────────────────────────────────────────────────────────────────────────

namespace {

std::unique_ptr<Order> make_order(OrderId id, Quantity qty, Price price = 100,
                                  Side side = Side::Buy) {
    auto o          = std::make_unique<Order>();
    o->id           = id;
    o->remaining_qty = qty;
    o->price        = price;
    o->side         = side;
    o->state        = OrderState::Active;
    return o;
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
// PriceLevel — append / remove / volume tracking
// ═════════════════════════════════════════════════════════════════════════════

TEST(PriceLevel, AppendSingle) {
    PriceLevel level{100};
    auto o = make_order(1, 10);

    level.append(o.get());

    EXPECT_EQ(level.head, o.get());
    EXPECT_EQ(level.tail, o.get());
    EXPECT_EQ(level.total_qty, 10u);
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(o->level, &level);
    EXPECT_EQ(o->prev, nullptr);
    EXPECT_EQ(o->next, nullptr);
    EXPECT_FALSE(level.empty());

    // cleanup
    level.remove(o.get());
}

TEST(PriceLevel, AppendMultiple_FIFOOrder) {
    PriceLevel level{100};
    auto o1 = make_order(1, 5);
    auto o2 = make_order(2, 7);
    auto o3 = make_order(3, 3);

    level.append(o1.get());
    level.append(o2.get());
    level.append(o3.get());

    EXPECT_EQ(level.head, o1.get());   // first in
    EXPECT_EQ(level.tail, o3.get());   // last in

    // list linkage
    EXPECT_EQ(o1->prev, nullptr);
    EXPECT_EQ(o1->next, o2.get());
    EXPECT_EQ(o2->prev, o1.get());
    EXPECT_EQ(o2->next, o3.get());
    EXPECT_EQ(o3->prev, o2.get());
    EXPECT_EQ(o3->next, nullptr);

    EXPECT_EQ(level.total_qty, 15u);
    EXPECT_EQ(level.order_count, 3u);

    level.remove(o1.get());
    level.remove(o2.get());
    level.remove(o3.get());
}

TEST(PriceLevel, RemoveHead) {
    PriceLevel level{100};
    auto o1 = make_order(1, 5);
    auto o2 = make_order(2, 7);

    level.append(o1.get());
    level.append(o2.get());

    level.remove(o1.get());

    EXPECT_EQ(level.head, o2.get());
    EXPECT_EQ(level.tail, o2.get());
    EXPECT_EQ(o2->prev, nullptr);
    EXPECT_EQ(level.total_qty, 7u);
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(o1->level, nullptr);
    EXPECT_EQ(o1->prev, nullptr);
    EXPECT_EQ(o1->next, nullptr);

    level.remove(o2.get());
}

TEST(PriceLevel, RemoveTail) {
    PriceLevel level{100};
    auto o1 = make_order(1, 5);
    auto o2 = make_order(2, 7);

    level.append(o1.get());
    level.append(o2.get());

    level.remove(o2.get());

    EXPECT_EQ(level.head, o1.get());
    EXPECT_EQ(level.tail, o1.get());
    EXPECT_EQ(o1->next, nullptr);
    EXPECT_EQ(level.total_qty, 5u);
    EXPECT_EQ(level.order_count, 1u);
    EXPECT_EQ(o2->level, nullptr);

    level.remove(o1.get());
}

TEST(PriceLevel, RemoveMiddle) {
    PriceLevel level{100};
    auto o1 = make_order(1, 5);
    auto o2 = make_order(2, 7);
    auto o3 = make_order(3, 3);

    level.append(o1.get());
    level.append(o2.get());
    level.append(o3.get());

    level.remove(o2.get());

    EXPECT_EQ(level.head, o1.get());
    EXPECT_EQ(level.tail, o3.get());
    EXPECT_EQ(o1->next, o3.get());
    EXPECT_EQ(o3->prev, o1.get());
    EXPECT_EQ(o2->level, nullptr);
    EXPECT_EQ(o2->prev, nullptr);
    EXPECT_EQ(o2->next, nullptr);
    EXPECT_EQ(level.total_qty, 8u);
    EXPECT_EQ(level.order_count, 2u);

    level.remove(o1.get());
    level.remove(o3.get());
}

TEST(PriceLevel, RemoveOnly) {
    PriceLevel level{100};
    auto o = make_order(1, 10);

    level.append(o.get());
    level.remove(o.get());

    EXPECT_EQ(level.head, nullptr);
    EXPECT_EQ(level.tail, nullptr);
    EXPECT_EQ(level.total_qty, 0u);
    EXPECT_EQ(level.order_count, 0u);
    EXPECT_TRUE(level.empty());
}

TEST(PriceLevel, VolumeTracking) {
    PriceLevel level{100};
    auto o1 = make_order(1, 10);
    auto o2 = make_order(2, 20);
    auto o3 = make_order(3, 30);

    level.append(o1.get());
    EXPECT_EQ(level.total_qty, 10u);

    level.append(o2.get());
    EXPECT_EQ(level.total_qty, 30u);

    level.append(o3.get());
    EXPECT_EQ(level.total_qty, 60u);

    level.remove(o2.get());
    EXPECT_EQ(level.total_qty, 40u);

    level.remove(o1.get());
    EXPECT_EQ(level.total_qty, 30u);

    level.remove(o3.get());
    EXPECT_EQ(level.total_qty, 0u);
}

TEST(PriceLevel, FrontReturnsMostRecentHead) {
    PriceLevel level{100};
    auto o1 = make_order(1, 5);
    auto o2 = make_order(2, 7);

    level.append(o1.get());
    level.append(o2.get());

    EXPECT_EQ(level.front(), o1.get());

    level.remove(o1.get());
    EXPECT_EQ(level.front(), o2.get());

    level.remove(o2.get());
    EXPECT_EQ(level.front(), nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
// BookSide — level management
// ═════════════════════════════════════════════════════════════════════════════

TEST(BookSide, GetOrCreate_CreatesThenReuses) {
    BookSide side{Side::Buy};

    PriceLevel* l1 = side.get_or_create_level(100);
    ASSERT_NE(l1, nullptr);
    EXPECT_EQ(l1->price, 100u);

    // second call returns the same level
    PriceLevel* l2 = side.get_or_create_level(100);
    EXPECT_EQ(l1, l2);

    EXPECT_FALSE(side.empty());
}

TEST(BookSide, FindLevel_ReturnsNullWhenAbsent) {
    BookSide side{Side::Sell};

    EXPECT_EQ(side.find_level(99), nullptr);
}

TEST(BookSide, FindLevel_ReturnsExistingLevel) {
    BookSide side{Side::Buy};
    side.get_or_create_level(200);

    EXPECT_NE(side.find_level(200), nullptr);
    EXPECT_EQ(side.find_level(200)->price, 200u);
    EXPECT_EQ(side.find_level(199), nullptr);
}

TEST(BookSide, EraseLevel_RemovesFromMap) {
    BookSide side{Side::Sell};
    side.get_or_create_level(50);
    EXPECT_NE(side.find_level(50), nullptr);

    side.erase_level(50);
    EXPECT_EQ(side.find_level(50), nullptr);
    EXPECT_TRUE(side.empty());
}

TEST(BookSide, BestLevel_AskIsLowest) {
    BookSide asks{Side::Sell};
    asks.get_or_create_level(103);
    asks.get_or_create_level(101);
    asks.get_or_create_level(102);

    const PriceLevel* best = asks.best_level();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->price, 101u);  // lowest ask
}

TEST(BookSide, BestLevel_BidIsHighest) {
    BookSide bids{Side::Buy};
    bids.get_or_create_level(98);
    bids.get_or_create_level(100);
    bids.get_or_create_level(99);

    const PriceLevel* best = bids.best_level();
    ASSERT_NE(best, nullptr);
    EXPECT_EQ(best->price, 100u);  // highest bid
}

TEST(BookSide, BestLevel_EmptyReturnsNull) {
    BookSide side{Side::Buy};
    EXPECT_EQ(side.best_level(), nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
// OrderBook — register / find / place / remove
// ═════════════════════════════════════════════════════════════════════════════

TEST(OrderBook, RegisterAndFind) {
    OrderBook book;

    auto o  = make_order(42, 10);
    Order* raw = book.register_order(std::move(o));

    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->id, 42u);
    EXPECT_TRUE(book.has_order(42));
    EXPECT_EQ(book.find(42), raw);
    EXPECT_EQ(book.find(99), nullptr);
}

TEST(OrderBook, UnregisterOrder_ReleasesOwnership) {
    OrderBook book;

    book.register_order(make_order(1, 5));
    EXPECT_TRUE(book.has_order(1));

    auto ptr = book.unregister_order(1);
    EXPECT_NE(ptr, nullptr);
    EXPECT_FALSE(book.has_order(1));
}

TEST(OrderBook, PlaceOnBook_BidSide) {
    OrderBook book;

    auto o   = make_order(1, 10, 100, Side::Buy);
    Order* raw = book.register_order(std::move(o));

    book.place_on_book(raw);

    ASSERT_NE(book.bids().best_level(), nullptr);
    EXPECT_EQ(book.bids().best_level()->price, 100u);
    EXPECT_EQ(book.bids().best_level()->total_qty, 10u);
    EXPECT_TRUE(book.asks().empty());
}

TEST(OrderBook, PlaceOnBook_AskSide) {
    OrderBook book;

    auto o   = make_order(1, 5, 200, Side::Sell);
    Order* raw = book.register_order(std::move(o));

    book.place_on_book(raw);

    ASSERT_NE(book.asks().best_level(), nullptr);
    EXPECT_EQ(book.asks().best_level()->price, 200u);
    EXPECT_EQ(book.asks().best_level()->total_qty, 5u);
    EXPECT_TRUE(book.bids().empty());
}

TEST(OrderBook, RemoveFromBook_CleansEmptyLevel) {
    OrderBook book;

    auto o   = make_order(1, 10, 100, Side::Buy);
    Order* raw = book.register_order(std::move(o));
    book.place_on_book(raw);

    book.remove_from_book(raw);

    EXPECT_TRUE(book.bids().empty());
    EXPECT_EQ(raw->level, nullptr);

    (void)book.unregister_order(1);
}

TEST(OrderBook, RemoveFromBook_PartialLevel_LevelSurvives) {
    OrderBook book;

    auto o1  = make_order(1, 10, 100, Side::Buy);
    auto o2  = make_order(2,  5, 100, Side::Buy);
    Order* r1 = book.register_order(std::move(o1));
    Order* r2 = book.register_order(std::move(o2));
    book.place_on_book(r1);
    book.place_on_book(r2);

    book.remove_from_book(r1);

    // level still exists; only o2 remains
    ASSERT_NE(book.bids().best_level(), nullptr);
    EXPECT_EQ(book.bids().best_level()->total_qty, 5u);
    EXPECT_EQ(book.bids().best_level()->order_count, 1u);

    book.remove_from_book(r2);
    (void)book.unregister_order(1);
    (void)book.unregister_order(2);
}

TEST(OrderBook, MultiplePriceLevels_BestUpdatesCorrectly) {
    OrderBook book;

    std::vector<std::pair<OrderId, Price>> bids = {{1, 99}, {2, 101}, {3, 100}};
    for (auto [id, price] : bids) {
        Order* r = book.register_order(make_order(id, 5, price, Side::Buy));
        book.place_on_book(r);
    }

    EXPECT_EQ(book.bids().best_level()->price, 101u);

    // remove the best bid
    book.remove_from_book(book.find(2));
    (void)book.unregister_order(2);
    EXPECT_EQ(book.bids().best_level()->price, 100u);

    // cleanup
    book.remove_from_book(book.find(1)); (void)book.unregister_order(1);
    book.remove_from_book(book.find(3)); (void)book.unregister_order(3);
}

#include <gtest/gtest.h>

#include "event.hpp"
#include "order.hpp"
#include "price_level.hpp"
#include "types.hpp"
#include "validation.hpp"

#include <cstddef>   // offsetof
#include <limits>

using namespace exchange;

// ═════════════════════════════════════════════════════════════════════════════
// Tip aliaslarının boyutları
// ─────────────────────────────────────────────────────────────────────────────
// uint64_t garantisi: tüm platformlarda tam 8 byte. int/long gibi platform
// bağımlı tipler değil, sabit genişlikli tipler kullanıldığını doğrular.
// ═════════════════════════════════════════════════════════════════════════════

TEST(TypeAlias, Sizes) {
    EXPECT_EQ(sizeof(OrderId),  8u);
    EXPECT_EQ(sizeof(Price),    8u);
    EXPECT_EQ(sizeof(Quantity), 8u);
    EXPECT_EQ(sizeof(SeqNo),    8u);
}

TEST(TypeAlias, AreUnsigned) {
    // İşaretsiz tip: sıfırdan bir düşürülünce max değere wrap eder, negatife gitmez.
    EXPECT_GT(OrderId{0}  - 1, OrderId{0});
    EXPECT_GT(Price{0}    - 1, Price{0});
    EXPECT_GT(Quantity{0} - 1, Quantity{0});
    EXPECT_GT(SeqNo{0}    - 1, SeqNo{0});
}

// ═════════════════════════════════════════════════════════════════════════════
// Enum underlying tipleri — hepsi : std::uint8_t → sizeof == 1
// ─────────────────────────────────────────────────────────────────────────────
// Order struct içinde birden fazla enum yan yana durduğunda padding hesabı
// bu küçük boyuta göre yapıldı. Biri 4 byte olsaydı layout patlar.
// ═════════════════════════════════════════════════════════════════════════════

TEST(EnumUnderlying, AllAreOneByte) {
    EXPECT_EQ(sizeof(Side),         1u);
    EXPECT_EQ(sizeof(OrderType),    1u);
    EXPECT_EQ(sizeof(OrderState),   1u);
    EXPECT_EQ(sizeof(EventType),    1u);
    EXPECT_EQ(sizeof(RejectReason), 1u);
}

// ═════════════════════════════════════════════════════════════════════════════
// Enum değerleri — her enumerator'ın sayısal değeri sabit kalmalı.
// Serialization / replay / event consumer tarafı bu sayısal değerlere güvenir.
// ═════════════════════════════════════════════════════════════════════════════

TEST(SideEnum, Values) {
    EXPECT_EQ(static_cast<uint8_t>(Side::Buy),  0u);
    EXPECT_EQ(static_cast<uint8_t>(Side::Sell), 1u);
    EXPECT_NE(Side::Buy, Side::Sell);
}

TEST(OrderTypeEnum, Values) {
    EXPECT_EQ(static_cast<uint8_t>(OrderType::Limit),  0u);
    EXPECT_EQ(static_cast<uint8_t>(OrderType::Market), 1u);
    EXPECT_NE(OrderType::Limit, OrderType::Market);
}

TEST(OrderStateEnum, Values) {
    EXPECT_EQ(static_cast<uint8_t>(OrderState::Accepted),        0u);
    EXPECT_EQ(static_cast<uint8_t>(OrderState::Active),          1u);
    EXPECT_EQ(static_cast<uint8_t>(OrderState::PartiallyFilled), 2u);
    EXPECT_EQ(static_cast<uint8_t>(OrderState::Filled),          3u);
    EXPECT_EQ(static_cast<uint8_t>(OrderState::Cancelled),       4u);
}

TEST(EventTypeEnum, Values) {
    EXPECT_EQ(static_cast<uint8_t>(EventType::OrderAccepted),        0u);
    EXPECT_EQ(static_cast<uint8_t>(EventType::OrderRested),          1u);
    EXPECT_EQ(static_cast<uint8_t>(EventType::OrderPartiallyFilled), 2u);
    EXPECT_EQ(static_cast<uint8_t>(EventType::OrderFilled),          3u);
    EXPECT_EQ(static_cast<uint8_t>(EventType::OrderCancelled),       4u);
    EXPECT_EQ(static_cast<uint8_t>(EventType::OrderModified),        5u);
    EXPECT_EQ(static_cast<uint8_t>(EventType::OrderRejected),        6u);
    EXPECT_EQ(static_cast<uint8_t>(EventType::TradeExecuted),        7u);
}

TEST(RejectReasonEnum, Values) {
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::None),             0u);
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::InvalidQuantity),  1u);
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::InvalidPrice),     2u);
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::InvalidSide),      3u);
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::DuplicateOrderId), 4u);
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::OrderNotFound),    5u);
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::OrderNotActive),   6u);
    EXPECT_EQ(static_cast<uint8_t>(RejectReason::InvalidModify),    7u);
}

// ═════════════════════════════════════════════════════════════════════════════
// Sentinel sabitler
// ═════════════════════════════════════════════════════════════════════════════

TEST(Sentinels, InvalidOrderId) {
    EXPECT_EQ(kInvalidOrderId, std::numeric_limits<OrderId>::max());
    EXPECT_GT(kInvalidOrderId, OrderId{0});
    // Hiçbir geçerli order bu id'yi alamaz — validation bunu reddetmeli.
}

TEST(Sentinels, InvalidPrice) {
    EXPECT_EQ(kInvalidPrice, Price{0});
    // Fiyat 0 geçersiz; validation price > 0 şartını uygular.
}

TEST(Sentinels, ZeroQty) {
    EXPECT_EQ(kZeroQty, Quantity{0});
}

// ═════════════════════════════════════════════════════════════════════════════
// Struct boyutları — static_assert'lerin runtime yansıması
// ─────────────────────────────────────────────────────────────────────────────
// static_assert derleme zamanında zaten yakaladı; bu testler ek güvence ve
// boyutun CI çıktısında görünür olmasını sağlar.
// ═════════════════════════════════════════════════════════════════════════════

TEST(StructSizes, Order) {
    EXPECT_EQ(sizeof(Order),   64u);    // tam 1 cache line
    EXPECT_EQ(alignof(Order),  64u);    // cache-line hizalı
}

TEST(StructSizes, PriceLevel) {
    EXPECT_EQ(sizeof(PriceLevel), 40u);
}

TEST(StructSizes, TradeEvent) {
    EXPECT_EQ(sizeof(TradeEvent), 48u);
}

TEST(StructSizes, OrderEvent) {
    EXPECT_EQ(sizeof(OrderEvent), 32u);
}

TEST(StructSizes, ValidationResult) {
    EXPECT_EQ(sizeof(ValidationResult), 2u);    // register'da taşınır, heap yok
}

// ═════════════════════════════════════════════════════════════════════════════
// Order field offset'leri — hot-path cache line tasarımını doğrular
// ─────────────────────────────────────────────────────────────────────────────
// Match loop'ta dokunulan her field ilk 48 byte içinde olmalı.
// offsetof ilk 6 field'ın sırasını garantiler.
// ═════════════════════════════════════════════════════════════════════════════

TEST(OrderLayout, HotFieldsInFirstCacheLine) {
    EXPECT_EQ(offsetof(Order, remaining_qty),  0u);
    EXPECT_EQ(offsetof(Order, id),             8u);
    EXPECT_EQ(offsetof(Order, prev),          16u);
    EXPECT_EQ(offsetof(Order, next),          24u);
    EXPECT_EQ(offsetof(Order, level),         32u);
    EXPECT_EQ(offsetof(Order, price),         40u);
}

TEST(OrderLayout, WarmFieldsInSameCacheLine) {
    EXPECT_EQ(offsetof(Order, seq_no), 48u);
    EXPECT_EQ(offsetof(Order, side),   56u);
    EXPECT_EQ(offsetof(Order, state),  57u);
    // Hepsi < 64 → hâlâ aynı cache line içinde.
    EXPECT_LT(offsetof(Order, state), 64u);
}

// ═════════════════════════════════════════════════════════════════════════════
// PriceLevel field offset'leri — match loop'ta en sık erişilen head/tail öndedir
// ═════════════════════════════════════════════════════════════════════════════

TEST(PriceLevelLayout, FieldOffsets) {
    EXPECT_EQ(offsetof(PriceLevel, head),        0u);
    EXPECT_EQ(offsetof(PriceLevel, tail),        8u);
    EXPECT_EQ(offsetof(PriceLevel, total_qty),  16u);
    EXPECT_EQ(offsetof(PriceLevel, price),      24u);
    EXPECT_EQ(offsetof(PriceLevel, order_count),32u);
}

// ═════════════════════════════════════════════════════════════════════════════
// TradeEvent ve OrderEvent field offset'leri
// ─────────────────────────────────────────────────────────────────────────────
// Alan sırası yeniden düzenlendi (8-byte alanlar öne) → gereksiz padding yok.
// ═════════════════════════════════════════════════════════════════════════════

TEST(TradeEventLayout, FieldOffsets) {
    EXPECT_EQ(offsetof(TradeEvent, seq_no),         0u);
    EXPECT_EQ(offsetof(TradeEvent, buy_order_id),   8u);
    EXPECT_EQ(offsetof(TradeEvent, sell_order_id), 16u);
    EXPECT_EQ(offsetof(TradeEvent, trade_price),   24u);
    EXPECT_EQ(offsetof(TradeEvent, trade_qty),     32u);
    EXPECT_EQ(offsetof(TradeEvent, aggressor_side),40u);
}

TEST(OrderEventLayout, FieldOffsets) {
    // 8-byte alanlar önde: 7-byte implicit padding ortadan kalktı → 40B'den 32B'ye düştü.
    EXPECT_EQ(offsetof(OrderEvent, seq_no),         0u);
    EXPECT_EQ(offsetof(OrderEvent, order_id),       8u);
    EXPECT_EQ(offsetof(OrderEvent, remaining_qty), 16u);
    EXPECT_EQ(offsetof(OrderEvent, type),          24u);
    EXPECT_EQ(offsetof(OrderEvent, reject_reason), 25u);
}

// ═════════════════════════════════════════════════════════════════════════════
// Order — varsayılan değerler ve helper metodlar
// ═════════════════════════════════════════════════════════════════════════════

TEST(Order, DefaultValues) {
    Order o{};
    EXPECT_EQ(o.remaining_qty, 0u);
    EXPECT_EQ(o.id,            0u);
    EXPECT_EQ(o.prev,          nullptr);
    EXPECT_EQ(o.next,          nullptr);
    EXPECT_EQ(o.level,         nullptr);
    EXPECT_EQ(o.price,         0u);
    EXPECT_EQ(o.seq_no,        0u);
    EXPECT_EQ(o.side,          Side::Buy);
    EXPECT_EQ(o.state,         OrderState::Accepted);
}

TEST(Order, IsActive_AllStates) {
    Order o{};

    o.state = OrderState::Accepted;
    EXPECT_FALSE(o.is_active());        // Accepted: henüz yerleşmemiş

    o.state = OrderState::Active;
    EXPECT_TRUE(o.is_active());         // book'ta bekliyor

    o.state = OrderState::PartiallyFilled;
    EXPECT_TRUE(o.is_active());         // kısmen dolu, hâlâ aktif

    o.state = OrderState::Filled;
    EXPECT_FALSE(o.is_active());        // tamamen bitti

    o.state = OrderState::Cancelled;
    EXPECT_FALSE(o.is_active());        // iptal edildi
}

TEST(Order, IsFullyFilled) {
    Order o{};

    o.remaining_qty = 100;
    EXPECT_FALSE(o.is_fully_filled());

    o.remaining_qty = 1;
    EXPECT_FALSE(o.is_fully_filled());

    o.remaining_qty = 0;
    EXPECT_TRUE(o.is_fully_filled());
}

TEST(Order, IsActive_And_IsFullyFilled_AreIndependent) {
    // State ve remaining_qty birbirinden bağımsız alanlar;
    // engine bunları tutarlı tutmakla yükümlü.
    Order o{};
    o.state         = OrderState::Active;
    o.remaining_qty = 0;
    // remaining_qty == 0 ama state henüz Active: engine geçişi tamamlamamış gibi
    EXPECT_TRUE(o.is_active());
    EXPECT_TRUE(o.is_fully_filled());
}

// ═════════════════════════════════════════════════════════════════════════════
// ValidationResult — factory metodlar
// ═════════════════════════════════════════════════════════════════════════════

TEST(ValidationResult, Accept) {
    const auto v = ValidationResult::accept();
    EXPECT_TRUE(v.ok);
    EXPECT_EQ(v.reason, RejectReason::None);
}

TEST(ValidationResult, Reject_CarriesReason) {
    const auto v = ValidationResult::reject(RejectReason::InvalidQuantity);
    EXPECT_FALSE(v.ok);
    EXPECT_EQ(v.reason, RejectReason::InvalidQuantity);
}

TEST(ValidationResult, EachRejectReasonPreserved) {
    // Tüm reject sebeplerinin factory üzerinden doğru aktarıldığını kontrol et.
    const RejectReason reasons[] = {
        RejectReason::InvalidQuantity,
        RejectReason::InvalidPrice,
        RejectReason::InvalidSide,
        RejectReason::DuplicateOrderId,
        RejectReason::OrderNotFound,
        RejectReason::OrderNotActive,
        RejectReason::InvalidModify,
    };
    for (auto r : reasons) {
        const auto v = ValidationResult::reject(r);
        EXPECT_FALSE(v.ok);
        EXPECT_EQ(v.reason, r);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Event factory helper'ları
// ═════════════════════════════════════════════════════════════════════════════

TEST(EventFactory, MakeTrade_FieldsSetCorrectly) {
    const auto e = make_trade(
        /*seq*/        SeqNo{1},
        /*buy_id*/     OrderId{42},
        /*sell_id*/    OrderId{99},
        /*price*/      Price{10000},   // 100.00 (tick = 0.01)
        /*qty*/        Quantity{5},
        /*aggressor*/  Side::Buy
    );

    EXPECT_EQ(e.seq_no,         SeqNo{1});
    EXPECT_EQ(e.buy_order_id,   OrderId{42});
    EXPECT_EQ(e.sell_order_id,  OrderId{99});
    EXPECT_EQ(e.trade_price,    Price{10000});
    EXPECT_EQ(e.trade_qty,      Quantity{5});
    EXPECT_EQ(e.aggressor_side, Side::Buy);
}

TEST(EventFactory, MakeOrderEvent_DefaultRejectReason) {
    const auto e = make_order_event(SeqNo{2}, OrderId{7},
                                    EventType::OrderFilled, Quantity{0});
    EXPECT_EQ(e.seq_no,        SeqNo{2});
    EXPECT_EQ(e.order_id,      OrderId{7});
    EXPECT_EQ(e.type,          EventType::OrderFilled);
    EXPECT_EQ(e.remaining_qty, Quantity{0});
    EXPECT_EQ(e.reject_reason, RejectReason::None);    // default
}

TEST(EventFactory, MakeOrderEvent_WithRejectReason) {
    const auto e = make_order_event(SeqNo{3}, OrderId{8},
                                    EventType::OrderRejected, Quantity{0},
                                    RejectReason::InvalidPrice);
    EXPECT_EQ(e.type,          EventType::OrderRejected);
    EXPECT_EQ(e.reject_reason, RejectReason::InvalidPrice);
    EXPECT_EQ(e.order_id,      OrderId{8});
    EXPECT_EQ(e.remaining_qty, Quantity{0});
}

TEST(EventFactory, MakeOrderEvent_RestedCarriesRemainingQty) {
    // ORDER_RESTED event'inde remaining_qty original qty'ye eşit olmalı.
    const auto e = make_order_event(SeqNo{4}, OrderId{1},
                                    EventType::OrderRested, Quantity{50});
    EXPECT_EQ(e.remaining_qty, Quantity{50});
    EXPECT_EQ(e.type,          EventType::OrderRested);
}

TEST(EventFactory, MakeTradeVsMakeOrderEvent_SizeDifference) {
    // TradeEvent (48B) vs OrderEvent (32B) — hem bellek hem Event variant boyutunu etkiler.
    EXPECT_GT(sizeof(TradeEvent), sizeof(OrderEvent));
    EXPECT_EQ(sizeof(TradeEvent), 48u);
    EXPECT_EQ(sizeof(OrderEvent), 32u);
}

#include "order_book.hpp"

#include "order.hpp"

#include <cassert>

namespace exchange {

// ── PriceLevel ────────────────────────────────────────────────────────────────

void PriceLevel::append(Order* o) noexcept {
    o->next = nullptr;
    o->level = this;

    if (tail == nullptr) {
        o->prev = nullptr;  // pool reuse durumunda stale pointer'a karşı savunma
        head    = o;
        tail    = o;
    } else {
        tail->next = o;
        o->prev    = tail;
        tail       = o;
    }

    total_qty += o->remaining_qty;
    ++order_count;
}

void PriceLevel::remove(Order* o) noexcept {
    assert(o->level == this);   // yanlış level'dan sökme girişimini yakala
    assert(order_count > 0);    // boş level'dan sökme girişimini yakala

    // ── Prev bağlantısını güncelle ────────────────────────────────────────────
    if (o->prev != nullptr) {
        o->prev->next = o->next;    // ortadan veya tail'den söküm
    } else {
        head = o->next;             // o head'di; yeni head bir sonraki
    }

    // ── Next bağlantısını güncelle ────────────────────────────────────────────
    if (o->next != nullptr) {
        o->next->prev = o->prev;    // ortadan veya head'den söküm
    } else {
        tail = o->prev;             // o tail'di; yeni tail bir önceki
    }

    // ── Level istatistiklerini güncelle ───────────────────────────────────────
    // Cancel: remaining_qty pozitif → level hacmi azalır.
    // Filled maker: match loop zaten trade_qty kadar düşürdü → remaining_qty == 0 → no-op.
    total_qty -= o->remaining_qty;
    --order_count;

    // ── Order pointer'larını temizle ──────────────────────────────────────────
    o->prev  = nullptr;
    o->next  = nullptr;
    o->level = nullptr;
}

// ── BookSide ──────────────────────────────────────────────────────────────────

PriceLevel* BookSide::get_or_create_level(Price price) {
    // try_emplace(key, ctor_args...):
    //   - key zaten varsa: mevcut node'a iterator döner, hiçbir şey oluşturmaz.
    //   - key yoksa: PriceLevel(price) in-place construct eder, iterator döner.
    // .first → iterator; .second → bool (inserted?). Bool'u kullanmıyoruz.
    return &(levels_.try_emplace(price, price).first->second);
}

PriceLevel* BookSide::find_level(Price price) noexcept {
    auto it = levels_.find(price);
    return (it != levels_.end()) ? &(it->second) : nullptr;
}

void BookSide::erase_level(Price price) {
    assert(levels_.count(price) > 0);   // var olmayan level'ı silme girişimini yakala
    levels_.erase(price);
}

PriceLevel* BookSide::best_level() noexcept {
    // 1. Defter boşsa en iyi seviye yoktur
    if (levels_.empty()) {
        return nullptr;
    }

    // 2. Tarafımıza göre en uç noktadaki seviyeyi seç
    if (side_ == Side::Buy) {
        // Alışlarda en yüksek fiyat (rbegin -> reverse begin) en iyidir.
        return &(levels_.rbegin()->second);
    } else {
        // Satışlarda en düşük fiyat (begin) en iyidir.
        return &(levels_.begin()->second);
    }
}

const PriceLevel* BookSide::best_level() const noexcept {
    if (levels_.empty()) {
        return nullptr;
    }
    if (side_ == Side::Buy) {
        return &(levels_.rbegin()->second);
    }
    return &(levels_.begin()->second);
}

// ── OrderBook ─────────────────────────────────────────────────────────────────

void OrderBook::place_on_book(Order* o) {
    assert(o != nullptr);
    assert(o->price > 0);           // market order'lar book'a girmez
    assert(o->remaining_qty > 0);   // dolu order book'a girmez

    BookSide& side = (o->side == Side::Buy) ? bids_ : asks_;
    side.get_or_create_level(o->price)->append(o);
}

void OrderBook::remove_from_book(Order* o) {
    assert(o != nullptr);
    assert(o->level != nullptr);    // book'ta olmayan order'ı çıkarmaya çalışma

    PriceLevel* level = o->level;   // remove() o->level'ı null'layacak, price'ı önceden sakla
    const Price price = level->price;

    level->remove(o);               // intrusive list'ten söker, o->level = nullptr yapar

    if (level->empty()) {
        BookSide& side = (o->side == Side::Buy) ? bids_ : asks_;
        side.erase_level(price);    // boş level'ı map'ten sil; 'level' artık dangling
    }
}

Order* OrderBook::register_order(std::unique_ptr<Order> o) {
    assert(o != nullptr);

    Order* raw = o.get();   // unique_ptr move edilmeden önce raw pointer'ı sakla

    // try_emplace: anahtar yoksa move ederek ekler; varsa hiç dokunmaz.
    // Validation zaten duplicate'i engelledi; assert sadece debug güvencesi.
    [[maybe_unused]] auto [it, inserted] = order_index_.try_emplace(o->id, std::move(o));
    assert(inserted);

    return raw;             // sahiplik order_index_'te, biz sadece erişim pointer'ı döndürürüz
}

std::unique_ptr<Order> OrderBook::unregister_order(OrderId id) {
    // extract(): node'u map'ten çıkarır ama belleği serbest bırakmaz → unique_ptr sağlıklı döner.
    // erase()'den farkı: değer kopyalanmaz/taşınmaz, node handle doğrudan transfer edilir.
    auto node = order_index_.extract(id);
    assert(!node.empty());          // var olmayan order'ı unregister etme girişimini yakala
    return std::move(node.mapped());
}

Order* OrderBook::find(OrderId id) const noexcept {
    auto it = order_index_.find(id);
    return (it != order_index_.end()) ? it->second.get() : nullptr;
}

bool OrderBook::has_order(OrderId id) const noexcept {
    return order_index_.contains(id);   // C++20; count(id) > 0 ile eşdeğer ama niyet daha net
}

}
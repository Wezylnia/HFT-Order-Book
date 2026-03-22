#include "snapshot.hpp"

#include "order.hpp"
#include "order_book.hpp"
#include "price_level.hpp"

#include <sstream>

namespace exchange {

BookSnapshot take_snapshot(const OrderBook& /*book*/) {
    // TODO M4: traverse bids and asks, build LevelSnapshot per level
    return {};
}

std::string dump_book(const OrderBook& /*book*/) {
    // TODO M4: human-readable multi-line representation
    return {};
}

}  // namespace exchange

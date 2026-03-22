#include "invariants.hpp"

#ifdef EXCHANGE_DEBUG

#include "order.hpp"
#include "order_book.hpp"
#include "price_level.hpp"

#include <cassert>

namespace exchange {

void check_invariants(const OrderBook& /*book*/) {
    // TODO M4: crossing check, level volumes, list coherence, membership, uniqueness
}

}  // namespace exchange

#endif  // EXCHANGE_DEBUG

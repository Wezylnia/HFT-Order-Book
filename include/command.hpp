#pragma once

#include "types.hpp"

namespace exchange {

// ── New limit order ───────────────────────────────────────────────────────────
struct NewLimitOrderCmd {
    OrderId  order_id{0};
    Side     side{Side::Buy};
    Price    price{0};
    Quantity quantity{0};
};

// ── New market order ──────────────────────────────────────────────────────────
// No price field — sweeps the opposite side at best available prices.
// Residual quantity is discarded (not placed on book).
struct NewMarketOrderCmd {
    OrderId  order_id{0};
    Side     side{Side::Buy};
    Quantity quantity{0};
};

// ── Cancel order ──────────────────────────────────────────────────────────────
struct CancelOrderCmd {
    OrderId target_order_id{0};
};

// ── Modify / replace order ────────────────────────────────────────────────────
// Semantics: cancel + new order (time priority resets unconditionally).
struct ModifyOrderCmd {
    OrderId  target_order_id{0};
    Price    new_price{0};
    Quantity new_quantity{0};
};

}  // namespace exchange
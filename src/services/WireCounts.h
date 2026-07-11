// =============================================================================
//  WireCounts.h - Conductor accounting per branch circuit (NBR 5410).
//
//  The graphic wiring distribution (EL-WIRE balloon) counts conductors PER
//  CIRCUIT that shares a conduit, NOT per component: the number of devices on a
//  circuit never multiplies its conductors, but two distinct circuits sharing
//  the same conduit add up. This header is the single source of truth for how
//  many Fase/Neutro/Terra/Retorno a given circuit type carries. It is BRX-free
//  and header-only so both the router and the unit tests use it directly.
//
//  Rule (residential 127/220 V, per the user's specification):
//    - Lighting circuit         : 1 Fase + 1 Neutro + 1 Retorno   (no PE run)
//    - General/Specific outlets : 1 Fase + 1 Neutro + 1 Terra (PE)
//    - Motor                    : 1 Fase + 1 Neutro + 1 Terra (PE)
//    - Mixed                    : 1 Fase + 1 Neutro + 1 Terra + 1 Retorno
// =============================================================================
#pragma once

#include "models/Types.h"

namespace electrical {

// Count of each conductor kind carried by a conduit (or one circuit).
struct ConductorSet {
    int phase   = 0;  // Fase (F)
    int neutral = 0;  // Neutro (N)
    int ground  = 0;  // Proteção / Terra (PE)
    int retorno = 0;  // Retorno (R)

    int total() const { return phase + neutral + ground + retorno; }

    ConductorSet& operator+=(const ConductorSet& o) {
        phase += o.phase; neutral += o.neutral;
        ground += o.ground; retorno += o.retorno;
        return *this;
    }
};

// Conductors carried by a single circuit of `type`.
inline ConductorSet conductorsFor(CircuitType type) {
    switch (type) {
        case CircuitType::Lighting:        return { 1, 1, 0, 1 };
        case CircuitType::GeneralOutlets:  return { 1, 1, 1, 0 };
        case CircuitType::SpecificOutlets: return { 1, 1, 1, 0 };
        case CircuitType::Motor:           return { 1, 1, 1, 0 };
        case CircuitType::Mixed:           return { 1, 1, 1, 1 };
    }
    return { 1, 1, 1, 0 };
}

} // namespace electrical

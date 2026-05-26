#pragma once

#include "combolens/abstract/SearchState.hpp"

namespace combolens {

struct RouteStats {
    int totalDamage = 0;
    int totalDifficulty = 0;
    int meterSpent = 0;
    int meterGained = 0;
};

struct Route {
    std::vector<MoveId> moves;
    RouteStats stats;
    SearchState finalState;
};

inline bool isEndRoute(const Route& r) {
    return !r.moves.empty() && r.moves.back() == MOVE_END;
}

} // namespace combolens

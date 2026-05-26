#pragma once

#include "combolens/abstract/AbstractTransition.hpp"
#include "combolens/search/Route.hpp"

namespace combolens {

struct SearchSettings {
    int maxDepth = MAX_COMBO_LEN;
    std::optional<MoveId> forcedStarter = std::nullopt;
    std::size_t routeLimit = 100000;
    bool enableStateMemo = false;
};

struct SearchResult {
    std::vector<Route> routes;
    std::size_t routeCount = 0;
    Route bestRoute;
    int bestDamage = 0;
};

struct SearchContext {
    const MoveDatabase& db;
    SearchSettings settings;
    SimSettings simSettings;
    std::unordered_set<SearchState, SearchStateHash> visitedAtDepth;
};

inline void recordRouteWithEnd(const SearchNode& node, const std::vector<MoveId>& prefix, const RouteStats& stats, SearchResult& out) {
    if (prefix.empty()) return;

    Route r;
    r.moves = prefix;
    r.moves.push_back(MOVE_END);
    r.stats = stats;
    r.finalState = node.abstract;

    out.routeCount++;
    if (out.routes.size() < out.routes.capacity()) {
        out.routes.push_back(r);
    } else if (out.routes.size() < 100000) {
        out.routes.push_back(r);
    }

    if (stats.totalDamage > out.bestDamage) {
        out.bestDamage = stats.totalDamage;
        out.bestRoute = r;
    }
}

inline void searchRoutesDfs(
    const SearchNode& node,
    std::vector<MoveId>& prefix,
    RouteStats stats,
    int depth,
    SearchContext& ctx,
    SearchResult& out
) {
    if (!prefix.empty()) {
        recordRouteWithEnd(node, prefix, stats, out);
    }

    if (depth >= ctx.settings.maxDepth) return;
    if (node.abstract.opp == SearchOppState::Knockdown) return;
    if (node.abstract.opp == SearchOppState::Neutral && node.abstract.lastMove != MOVE_NONE) return;

    for (const MoveDef& move : ctx.db.moves()) {
        if (depth == 0 && ctx.settings.forcedStarter.has_value() && move.id != *ctx.settings.forcedStarter) {
            continue;
        }

        MoveDef candidate = move;
        if (depth == 0 && ctx.settings.forcedStarter.has_value() && move.id == *ctx.settings.forcedStarter) {
            candidate.canStartCombo = true;
        }

        TransitionResult tr = tryApplyMoveHybrid(node, candidate, ctx.db, ctx.simSettings);
        if (!tr.valid) continue;

        SearchNode nextNode{tr.next, tr.nextRepresentativeFrame};

        RouteStats nextStats = stats;
        nextStats.totalDamage += tr.damage;
        nextStats.totalDifficulty += tr.difficulty;
        nextStats.meterSpent += move.onHit.meterCost + move.meterCost;
        nextStats.meterGained += move.onHit.meterGain;

        prefix.push_back(move.id);
        searchRoutesDfs(nextNode, prefix, nextStats, depth + 1, ctx, out);
        prefix.pop_back();
    }
}

inline SearchResult searchRoutes(
    const SearchNode& initial,
    const MoveDatabase& db,
    SearchSettings settings = {},
    SimSettings simSettings = {}
) {
    SearchResult out;
    out.routes.reserve(std::min<std::size_t>(settings.routeLimit, 100000));

    SearchContext ctx{db, settings, simSettings, {}};
    std::vector<MoveId> prefix;
    RouteStats stats;
    searchRoutesDfs(initial, prefix, stats, 0, ctx, out);
    return out;
}

} // namespace combolens

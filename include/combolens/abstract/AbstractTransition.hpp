#pragma once

#include "combolens/abstract/Abstraction.hpp"
#include "combolens/frame/FrameSimulator.hpp"

namespace combolens {

struct TransitionResult {
    bool valid = false;
    SearchState next;
    FrameState nextRepresentativeFrame;
    int damage = 0;
    int difficulty = 0;
    bool hitVerified = false;
};

inline bool abstractRequirementsPass(const SearchState& s, const MoveDef& m) {
    if (s.opp == SearchOppState::Knockdown) return false;
    const bool starterFromNeutral = (s.opp == SearchOppState::Neutral && s.lastMove == MOVE_NONE && m.canStartCombo);
    if (s.opp == SearchOppState::Neutral && !starterFromNeutral) return false;

    if (m.requiresGroundedSelf && s.self != SearchSelfState::Grounded) return false;
    if (m.requiresAirborneSelf && s.self != SearchSelfState::Airborne) return false;

    if (m.requiresGroundedOpponent && !(s.opp == SearchOppState::Grounded || starterFromNeutral)) return false;
    if (m.requiresAirborneOpponent && s.opp != SearchOppState::Airborne) return false;
    if (m.requiresWallSplatOpponent && s.opp != SearchOppState::WallSplat) return false;

    if (s.meter * METER_BUCKET_SIZE < m.meterCost) return false;
    if (s.juggle + m.onHit.juggleCost > MAX_JUGGLE) return false;
    if (m.onHit.usesWallBounce && s.wallBounceUsed) return false;
    if (m.onHit.usesGroundBounce && s.groundBounceUsed) return false;

    return true;
}

inline bool canCancelFrom(MoveId previous, const MoveDef& next, const MoveDatabase& db) {
    if (previous == MOVE_NONE || previous == MOVE_END) return false;
    if (moveInList(previous, next.cancelFromMoves)) return true;
    const MoveCategory prevCat = db.byId(previous).category;
    return categoryInList(prevCat, next.cancelFromCategories);
}

inline CancelMask cancelMaskNeededForCategory(MoveCategory category) {
    switch (category) {
        case MoveCategory::Normal:
        case MoveCategory::Launcher:
        case MoveCategory::AirNormal:
            return cancelBit(CancelType::Normal) | cancelBit(CancelType::Jump);
        case MoveCategory::Special:
            return cancelBit(CancelType::Special);
        case MoveCategory::Super:
            return cancelBit(CancelType::Super);
    }
    return 0;
}

inline bool timingAllowsMove(const SearchState& s, const MoveDef& m, const MoveDatabase& db) {
    const bool starter = s.lastMove == MOVE_NONE && m.canStartCombo;

    bool cancel = false;
    if (s.cancelWindowRemaining > 0 && canCancelFrom(s.lastMove, m, db)) {
        const CancelMask needed = cancelMaskNeededForCategory(m.category);
        // Keep direct cancelFromMoves as permissive, but category cancels should match mask.
        const bool direct = moveInList(s.lastMove, m.cancelFromMoves);
        cancel = direct || ((s.availableCancels & needed) != 0);
    }

    const bool link = s.selfActionableIn == 0 && m.startup <= s.oppHitstunRemaining;
    return starter || cancel || link;
}

inline int scalingPercent(std::uint8_t scalingBucket) {
    return std::max(20, 100 - static_cast<int>(scalingBucket) * 10);
}

inline int computeDamage(const SearchState& s, const MoveDef& move) {
    return move.onHit.damage * scalingPercent(s.scaling) / 100;
}

inline void prepareFrameStateForMove(FrameState& f, const SearchState& s, const MoveDef& move) {
    f.self.currentMove = move.id;
    f.self.moveFrame = 0;
    f.self.phase = ActionPhase::Startup;
    f.self.recoveryRemaining = 0;
    f.self.hitstopRemaining = 0;
    f.opp.hitstopRemaining = 0;

    f.opp.hitstunRemaining = s.oppHitstunRemaining;
    f.meter = s.meter * METER_BUCKET_SIZE;
    f.scalingBucket = s.scaling;
    f.juggle = s.juggle;
    f.wallBounceUsed = s.wallBounceUsed;
    f.groundBounceUsed = s.groundBounceUsed;
    f.lastMove = s.lastMove;

    if (s.self == SearchSelfState::Grounded) {
        f.self.body = BodyState::Grounded;
        f.self.pos.y = 0.0f;
    } else {
        f.self.body = BodyState::Airborne;
        f.self.pos.y = std::max(f.self.pos.y, 40.0f);
    }

    if (s.opp == SearchOppState::Grounded || s.opp == SearchOppState::WallSplat) {
        f.opp.body = BodyState::Grounded;
        f.opp.pos.y = 0.0f;
        f.opp.phase = ActionPhase::Hitstun;
    } else if (s.opp == SearchOppState::Airborne) {
        f.opp.body = BodyState::Airborne;
        f.opp.phase = ActionPhase::Hitstun;
        f.opp.pos.y = std::max(f.opp.pos.y, static_cast<float>(s.height) * 40.0f);
    }

    f.opponentWallSplat = (s.opp == SearchOppState::WallSplat);

    // Nudge airborne attacker toward airborne opponent for simplified v1 air-route continuity.
    if (f.self.body == BodyState::Airborne && f.opp.body == BodyState::Airborne) {
        f.self.pos.y = std::max(40.0f, f.opp.pos.y - 10.0f);
    }
}

inline TransitionResult tryApplyMoveHybrid(const SearchNode& node, const MoveDef& move, const MoveDatabase& db, SimSettings simSettings = {}) {
    TransitionResult out;
    const SearchState& s = node.abstract;

    if (!abstractRequirementsPass(s, move)) return out;
    if (!timingAllowsMove(s, move, db)) return out;

    FrameState start = node.representative;
    prepareFrameStateForMove(start, s, move);

    SimResult sim = simulateMoveFromState(start, move, simSettings);
    if (!sim.hit) return out;

    out.valid = true;
    out.nextRepresentativeFrame = sim.finalFrameState;
    out.next = abstractFromFrameState(sim.finalFrameState, db);
    out.damage = computeDamage(s, move);
    out.difficulty = move.difficulty;
    out.hitVerified = true;
    return out;
}

} // namespace combolens

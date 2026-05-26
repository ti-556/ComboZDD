#pragma once

#include "combolens/abstract/SearchState.hpp"

namespace combolens {

inline std::uint8_t bucketMeter(int meter) {
    return clampU8(meter / METER_BUCKET_SIZE, 0, MAX_METER_BUCKET);
}

inline std::uint8_t bucketScaling(int scalingBucket) {
    return clampU8(scalingBucket, 0, MAX_SCALING_BUCKET);
}

inline std::uint8_t bucketHeight(float y) {
    return clampU8(static_cast<int>(std::round(y / 40.0f)), 0, 10);
}

inline std::uint8_t bucketDistance(float dx) {
    return clampU8(static_cast<int>(std::round(std::abs(dx) / 50.0f)), 0, 10);
}

inline std::uint8_t bucketWallDist(float x) {
    const float distLeft = std::abs(x - static_cast<float>(STAGE_LEFT));
    const float distRight = std::abs(static_cast<float>(STAGE_RIGHT) - x);
    const float nearest = std::min(distLeft, distRight);
    return clampU8(static_cast<int>(std::round(nearest / 50.0f)), 0, 10);
}

inline std::uint8_t bucketFrames(int frames) {
    return clampU8(frames, 0, 60);
}

inline SearchSelfState abstractSelf(const CharacterFrameState& c) {
    return c.body == BodyState::Airborne ? SearchSelfState::Airborne : SearchSelfState::Grounded;
}

inline SearchOppState abstractOpp(const FrameState& f) {
    if (f.opp.body == BodyState::Knockdown || f.opp.phase == ActionPhase::Knockdown) {
        return SearchOppState::Knockdown;
    }
    if (f.opponentWallSplat) {
        return SearchOppState::WallSplat;
    }
    if (f.opp.body == BodyState::Airborne || f.opp.pos.y > 0.0f) {
        return SearchOppState::Airborne;
    }
    if (f.opp.hitstunRemaining > 0) {
        return SearchOppState::Grounded;
    }
    return SearchOppState::Neutral;
}

inline CancelMask computeAvailableCancels(const FrameState& f, const MoveDatabase& db) {
    if (f.self.currentMove == MOVE_NONE) return 0;
    if (f.self.currentMove == MOVE_END) return 0;

    const MoveDef& move = db.byId(f.self.currentMove);
    for (const auto& win : move.cancelWindows) {
        if (f.self.moveFrame >= win.startFrame && f.self.moveFrame <= win.endFrame) {
            return win.mask;
        }
    }
    return 0;
}

inline std::uint8_t computeCancelWindowRemaining(const FrameState& f, const MoveDatabase& db) {
    if (f.self.currentMove == MOVE_NONE) return 0;
    const MoveDef& move = db.byId(f.self.currentMove);
    for (const auto& win : move.cancelWindows) {
        if (f.self.moveFrame >= win.startFrame && f.self.moveFrame <= win.endFrame) {
            return bucketFrames(win.endFrame - f.self.moveFrame + 1);
        }
    }
    return 0;
}

inline bool decisionPossible(const SearchState& s) {
    if (s.opp == SearchOppState::Knockdown || s.opp == SearchOppState::Neutral) return false;
    if (s.cancelWindowRemaining > 0 && s.availableCancels != 0) return true;
    if (s.selfActionableIn == 0 && s.oppHitstunRemaining > 0) return true;
    if (s.lastMove == MOVE_NONE) return true;
    return false;
}

inline SearchState canonicalize(SearchState s) {
    if (s.opp == SearchOppState::Knockdown || s.opp == SearchOppState::Neutral) {
        s.selfActionableIn = 0;
        s.oppHitstunRemaining = 0;
        s.cancelWindowRemaining = 0;
        s.availableCancels = 0;
        return s;
    }

    if (s.cancelWindowRemaining > 0 && s.availableCancels != 0) return s;
    if (s.selfActionableIn == 0) return s;

    const std::uint8_t dt = std::min(s.selfActionableIn, s.oppHitstunRemaining);
    s.selfActionableIn -= dt;
    s.oppHitstunRemaining -= dt;

    if (s.oppHitstunRemaining == 0 && s.lastMove != MOVE_NONE) {
        s.opp = SearchOppState::Neutral;
        s.cancelWindowRemaining = 0;
        s.availableCancels = 0;
    }
    return s;
}

inline SearchState abstractFromFrameState(const FrameState& f, const MoveDatabase& db) {
    SearchState s;
    s.self = abstractSelf(f.self);
    s.opp = abstractOpp(f);

    s.selfActionableIn = bucketFrames(std::max(0, f.self.recoveryRemaining));
    s.oppHitstunRemaining = bucketFrames(std::max(0, f.opp.hitstunRemaining));
    s.availableCancels = computeAvailableCancels(f, db);
    s.cancelWindowRemaining = computeCancelWindowRemaining(f, db);

    s.height = bucketHeight(f.opp.pos.y);
    s.distance = bucketDistance(f.opp.pos.x - f.self.pos.x);
    s.wallDist = bucketWallDist(f.opp.pos.x);

    s.meter = bucketMeter(f.meter);
    s.scaling = bucketScaling(f.scalingBucket);
    s.juggle = clampU8(f.juggle, 0, MAX_JUGGLE);

    s.wallBounceUsed = f.wallBounceUsed;
    s.groundBounceUsed = f.groundBounceUsed;
    s.lastMove = f.lastMove;

    return canonicalize(s);
}

} // namespace combolens

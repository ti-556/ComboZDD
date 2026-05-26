#pragma once

#include "combolens/frame/FrameState.hpp"

namespace combolens {

inline bool moveFrameActive(const MoveDef& move, int moveFrame) {
    for (const auto& hb : move.hitboxes) {
        if (moveFrame >= hb.startFrame && moveFrame <= hb.endFrame) {
            return true;
        }
    }
    return false;
}

inline std::vector<Rect> activeHitboxes(const CharacterFrameState& self, const MoveDef& move) {
    std::vector<Rect> boxes;
    for (const auto& hb : move.hitboxes) {
        if (self.moveFrame >= hb.startFrame && self.moveFrame <= hb.endFrame) {
            boxes.push_back(worldRect(hb.boxLocal, self));
        }
    }
    return boxes;
}

inline bool checkHit(const FrameState& s, const MoveDef& move) {
    Rect oppHurt = worldHurtbox(s.opp);
    for (const auto& hb : activeHitboxes(s.self, move)) {
        if (overlaps(hb, oppHurt)) {
            return true;
        }
    }
    return false;
}

inline void updateMotion(CharacterFrameState& c, const SimSettings& settings) {
    c.pos.x += c.vel.x;
    c.pos.y += c.vel.y;

    if (c.body == BodyState::Airborne) {
        c.vel.y += settings.gravity;
        c.vel.x *= settings.airFriction;
    } else {
        c.vel.x *= settings.groundFriction;
    }

    if (c.pos.y <= FLOOR_Y) {
        c.pos.y = static_cast<float>(FLOOR_Y);
        if (c.vel.y < 0.0f) c.vel.y = 0.0f;
        if (c.body == BodyState::Airborne) {
            c.body = BodyState::Grounded;
        }
    }

    c.pos.x = clampFloat(c.pos.x, static_cast<float>(STAGE_LEFT), static_cast<float>(STAGE_RIGHT));
}

inline void updateTimers(CharacterFrameState& c) {
    if (c.hitstunRemaining > 0) {
        c.hitstunRemaining--;
        c.phase = (c.hitstunRemaining > 0) ? ActionPhase::Hitstun : ActionPhase::Idle;
    }
    if (c.recoveryRemaining > 0) {
        c.recoveryRemaining--;
        if (c.recoveryRemaining == 0 && c.phase == ActionPhase::Recovery) {
            c.phase = ActionPhase::Idle;
        }
    }
}

inline void applyHit(FrameState& s, const MoveDef& move) {
    const HitEffect& h = move.onHit;

    s.opp.hitstunRemaining = h.hitstun;
    s.opp.phase = ActionPhase::Hitstun;
    s.opp.hitstopRemaining = h.hitstop;
    s.self.hitstopRemaining = h.hitstop;

    s.opp.vel = h.launchVelocity;
    if (!s.self.facingRight) {
        s.opp.vel.x *= -1.0f;
    }
    if (h.launchVelocity.y > 0.0f || s.opp.pos.y > FLOOR_Y) {
        s.opp.body = BodyState::Airborne;
    }

    if (h.selfBecomesAirborne) {
        s.self.body = BodyState::Airborne;
        s.self.pos.y = std::max(s.self.pos.y, 60.0f);
        s.self.vel.y = std::max(s.self.vel.y, 2.0f);
    }

    if (h.selfBecomesGrounded) {
        s.self.body = BodyState::Grounded;
        s.self.pos.y = 0.0f;
        s.self.vel.y = 0.0f;
    }

    s.self.recoveryRemaining = move.recovery;
    s.self.phase = ActionPhase::Recovery;

    s.meter = clampInt(s.meter + h.meterGain - h.meterCost, 0, MAX_METER);
    s.scalingBucket = clampInt(s.scalingBucket + h.scalingStep, 0, MAX_SCALING_BUCKET);
    s.juggle = clampInt(s.juggle + h.juggleCost, 0, MAX_JUGGLE);

    if (h.usesWallBounce) s.wallBounceUsed = true;
    if (h.usesGroundBounce) s.groundBounceUsed = true;

    if (h.causesGroundBounce) {
        s.groundBounceUsed = true;
        s.opp.body = BodyState::Airborne;
        s.opp.vel.y = std::max(s.opp.vel.y, 4.0f);
    }

    if (h.causesKnockdown) {
        s.opp.body = BodyState::Knockdown;
        s.opp.phase = ActionPhase::Knockdown;
        s.opp.hitstunRemaining = 0;
        s.opp.vel = {0.0f, 0.0f};
    }

    s.lastMove = move.id;
}

inline void checkWallSplat(FrameState& s, const MoveDef& move, const SimSettings& settings) {
    if (!move.onHit.causesWallSplat) return;

    // V1 simplification: a wall-splat move snaps the opponent to the nearest wall.
    // Later versions can require actual wall contact before this state transition.
    const float distLeft = std::abs(s.opp.pos.x - static_cast<float>(STAGE_LEFT));
    const float distRight = std::abs(static_cast<float>(STAGE_RIGHT) - s.opp.pos.x);
    s.opp.pos.x = (distLeft < distRight) ? static_cast<float>(STAGE_LEFT + 25) : static_cast<float>(STAGE_RIGHT - 25);

    s.opponentWallSplat = true;
    s.opp.body = BodyState::Grounded;
    s.opp.phase = ActionPhase::Hitstun;
    s.opp.vel = {0.0f, 0.0f};
    s.opp.pos.y = 0.0f;
    s.opp.hitstunRemaining += static_cast<int>(settings.wallSplatExtraHitstun);
    s.wallBounceUsed = true;
}

inline FrameDebugSnapshot makeSnapshot(const FrameState& s, const MoveDef& move, bool hit) {
    FrameDebugSnapshot snap;
    snap.frame = s.globalFrame;
    snap.currentMove = move.id;
    snap.moveFrame = s.self.moveFrame;
    snap.selfPos = s.self.pos;
    snap.oppPos = s.opp.pos;
    snap.selfHurtbox = worldHurtbox(s.self);
    snap.oppHurtbox = worldHurtbox(s.opp);
    snap.activeHitboxes = activeHitboxes(s.self, move);
    snap.hitConnected = hit;
    return snap;
}

inline SimResult simulateMoveFromState(FrameState start, const MoveDef& move, SimSettings settings = {}) {
    SimResult result;
    FrameState s = start;

    s.self.currentMove = move.id;
    s.self.moveFrame = 0;
    s.self.phase = ActionPhase::Startup;
    s.self.recoveryRemaining = move.startup + move.active + move.recovery;

    const int totalMoveFrames = move.startup + move.active + move.recovery;
    const int maxFrames = std::min(settings.maxFramesPerMove, totalMoveFrames + 60);

    bool alreadyHit = false;

    for (int i = 0; i < maxFrames; ++i) {
        if (s.self.hitstopRemaining > 0 || s.opp.hitstopRemaining > 0) {
            s.self.hitstopRemaining = std::max(0, s.self.hitstopRemaining - 1);
            s.opp.hitstopRemaining = std::max(0, s.opp.hitstopRemaining - 1);
            result.trace.push_back(makeSnapshot(s, move, false));
            s.globalFrame++;
            continue;
        }

        // Phase assignment for the acting character.
        if (s.self.moveFrame < move.startup) {
            s.self.phase = ActionPhase::Startup;
        } else if (s.self.moveFrame < move.startup + move.active) {
            s.self.phase = ActionPhase::Active;
        } else {
            s.self.phase = ActionPhase::Recovery;
        }

        updateMotion(s.self, settings);
        updateMotion(s.opp, settings);
        updateTimers(s.opp);

        bool hitThisFrame = false;
        if (!alreadyHit && s.self.phase == ActionPhase::Active && checkHit(s, move)) {
            applyHit(s, move);
            checkWallSplat(s, move, settings);
            alreadyHit = true;
            hitThisFrame = true;
            result.hit = true;
            result.hitFrame = s.globalFrame;
        }

        result.trace.push_back(makeSnapshot(s, move, hitThisFrame));

        s.self.moveFrame++;
        s.globalFrame++;

        if (alreadyHit) {
            // Stop at the meaningful outcome, not after every recovery frame.
            break;
        }
    }

    result.whiff = !result.hit;
    result.finalFrame = s.globalFrame;
    result.finalFrameState = s;
    result.comboStillValid = result.hit && s.opp.hitstunRemaining > 0 && s.opp.body != BodyState::Knockdown;
    return result;
}

} // namespace combolens

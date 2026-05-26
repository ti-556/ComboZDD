#pragma once

#include "combolens/core/MoveDef.hpp"

namespace combolens {

struct CharacterFrameState {
    Vec2 pos;
    Vec2 vel;

    BodyState body = BodyState::Grounded;
    ActionPhase phase = ActionPhase::Idle;

    MoveId currentMove = MOVE_NONE;
    int moveFrame = 0;

    int hitstunRemaining = 0;
    int hitstopRemaining = 0;
    int recoveryRemaining = 0;

    bool facingRight = true;
    Rect hurtboxLocal{-15.0f, 0.0f, 30.0f, 80.0f};
};

struct FrameState {
    CharacterFrameState self;
    CharacterFrameState opp;

    int globalFrame = 0;

    int meter = 0;
    int scalingBucket = 0;
    int juggle = 0;

    bool wallBounceUsed = false;
    bool groundBounceUsed = false;
    bool opponentWallSplat = false;

    MoveId lastMove = MOVE_NONE;
};

struct FrameDebugSnapshot {
    int frame = 0;
    MoveId currentMove = MOVE_NONE;
    int moveFrame = 0;
    Vec2 selfPos;
    Vec2 oppPos;
    Rect selfHurtbox;
    Rect oppHurtbox;
    std::vector<Rect> activeHitboxes;
    bool hitConnected = false;
};

struct SimSettings {
    int maxFramesPerMove = 180;
    float gravity = -0.45f;
    float groundFriction = 0.75f;
    float airFriction = 0.99f;
    float wallSplatExtraHitstun = 20.0f;
};

struct SimResult {
    bool hit = false;
    bool whiff = false;
    bool comboStillValid = false;
    int hitFrame = -1;
    int finalFrame = 0;
    FrameState finalFrameState;
    std::vector<FrameDebugSnapshot> trace;
};

inline Rect worldRect(const Rect& local, const CharacterFrameState& c) {
    Rect authored = c.facingRight ? local : flippedLocalRect(local);
    return Rect{c.pos.x + authored.x, c.pos.y + authored.y, authored.w, authored.h};
}

inline Rect worldHurtbox(const CharacterFrameState& c) {
    return worldRect(c.hurtboxLocal, c);
}

} // namespace combolens

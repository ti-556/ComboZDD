#pragma once

#include "combolens/core/MoveDef.hpp"
#include "combolens/frame/FrameState.hpp"

namespace combolens {

inline MoveDef make2M() {
    MoveDef m;
    m.id = 2;
    m.name = "2M";
    m.label = "Crouching Medium";
    m.category = MoveCategory::Normal;
    m.startup = 7;
    m.active = 3;
    m.recovery = 14;
    m.canStartCombo = true;
    m.requiresGroundedSelf = true;
    m.requiresGroundedOpponent = true;
    m.difficulty = 1;
    m.hitboxes.push_back({7, 9, Rect{20, 20, 60, 22}});
    m.cancelWindows.push_back({7, 15, static_cast<CancelMask>(cancelBit(CancelType::Normal) | cancelBit(CancelType::Special))});
    m.onHit.damage = 40;
    m.onHit.hitstun = 24;
    m.onHit.hitstop = 4;
    m.onHit.launchVelocity = {0.8f, 0.0f};
    m.onHit.meterGain = 20;
    m.onHit.scalingStep = 1;
    m.onHit.juggleCost = 1;
    return m;
}

inline MoveDef make5H(MoveId id2M) {
    MoveDef m;
    m.id = 3;
    m.name = "5H";
    m.label = "Standing Heavy";
    m.category = MoveCategory::Normal;
    m.startup = 10;
    m.active = 4;
    m.recovery = 20;
    m.requiresGroundedSelf = true;
    m.requiresGroundedOpponent = true;
    m.cancelFromMoves = {id2M};
    m.difficulty = 2;
    m.hitboxes.push_back({10, 13, Rect{22, 28, 78, 30}});
    m.cancelWindows.push_back({10, 18, static_cast<CancelMask>(cancelBit(CancelType::Normal) | cancelBit(CancelType::Special))});
    m.onHit.damage = 70;
    m.onHit.hitstun = 28;
    m.onHit.hitstop = 5;
    m.onHit.launchVelocity = {1.0f, 0.0f};
    m.onHit.meterGain = 20;
    m.onHit.scalingStep = 2;
    m.onHit.juggleCost = 2;
    return m;
}

inline MoveDef make2H(MoveId id5H) {
    MoveDef m;
    m.id = 4;
    m.name = "2H";
    m.label = "Launcher";
    m.category = MoveCategory::Launcher;
    m.startup = 11;
    m.active = 4;
    m.recovery = 24;
    m.requiresGroundedSelf = true;
    m.requiresGroundedOpponent = true;
    m.cancelFromMoves = {id5H};
    m.difficulty = 2;
    m.hitboxes.push_back({11, 14, Rect{18, 25, 70, 65}});
    m.cancelWindows.push_back({11, 20, static_cast<CancelMask>(cancelBit(CancelType::Jump) | cancelBit(CancelType::Normal))});
    m.onHit.damage = 65;
    m.onHit.hitstun = 40;
    m.onHit.hitstop = 5;
    m.onHit.launchVelocity = {1.1f, 8.5f};
    m.onHit.meterGain = 20;
    m.onHit.scalingStep = 2;
    m.onHit.juggleCost = 2;
    m.onHit.selfBecomesAirborne = true;
    return m;
}

inline MoveDef makeJM(MoveId id2H) {
    MoveDef m;
    m.id = 5;
    m.name = "jM";
    m.label = "Jump Medium";
    m.category = MoveCategory::AirNormal;
    m.startup = 6;
    m.active = 4;
    m.recovery = 12;
    m.requiresAirborneSelf = true;
    m.requiresAirborneOpponent = true;
    m.cancelFromMoves = {id2H};
    m.difficulty = 2;
    m.hitboxes.push_back({6, 9, Rect{15, 20, 75, 45}});
    m.cancelWindows.push_back({6, 13, static_cast<CancelMask>(cancelBit(CancelType::Normal) | cancelBit(CancelType::Special))});
    m.onHit.damage = 38;
    m.onHit.hitstun = 30;
    m.onHit.hitstop = 4;
    m.onHit.launchVelocity = {1.0f, 5.2f};
    m.onHit.meterGain = 15;
    m.onHit.scalingStep = 1;
    m.onHit.juggleCost = 1;
    return m;
}

inline MoveDef makeJH(MoveId idJM) {
    MoveDef m;
    m.id = 6;
    m.name = "jH";
    m.label = "Jump Heavy";
    m.category = MoveCategory::AirNormal;
    m.startup = 8;
    m.active = 4;
    m.recovery = 16;
    m.requiresAirborneSelf = true;
    m.requiresAirborneOpponent = true;
    m.cancelFromMoves = {idJM};
    m.difficulty = 2;
    m.hitboxes.push_back({8, 11, Rect{18, 18, 90, 50}});
    m.cancelWindows.push_back({8, 14, cancelBit(CancelType::Special)});
    m.onHit.damage = 60;
    m.onHit.hitstun = 32;
    m.onHit.hitstop = 4;
    m.onHit.launchVelocity = {1.1f, 2.8f};
    m.onHit.meterGain = 15;
    m.onHit.scalingStep = 2;
    m.onHit.juggleCost = 1;
    return m;
}

inline MoveDef make236H() {
    MoveDef m;
    m.id = 7;
    m.name = "236H";
    m.label = "Heavy Wall Special";
    m.category = MoveCategory::Special;
    m.startup = 10;
    m.active = 5;
    m.recovery = 22;
    m.requiresAirborneSelf = true;
    m.requiresAirborneOpponent = true;
    m.cancelFromCategories = {MoveCategory::AirNormal};
    m.difficulty = 3;
    m.hitboxes.push_back({10, 14, Rect{20, 15, 120, 60}});
    m.cancelWindows.push_back({10, 18, cancelBit(CancelType::Super)});
    m.onHit.damage = 90;
    m.onHit.hitstun = 38;
    m.onHit.hitstop = 6;
    m.onHit.launchVelocity = {10.0f, -1.0f};
    m.onHit.meterGain = 20;
    m.onHit.scalingStep = 2;
    m.onHit.juggleCost = 2;
    m.onHit.causesWallSplat = true;
    m.onHit.usesWallBounce = true;
    m.onHit.selfBecomesGrounded = true;
    return m;
}

inline MoveDef makeSuper1(MoveId id236H) {
    MoveDef m;
    m.id = 8;
    m.name = "Super1";
    m.label = "One-Bar Super";
    m.category = MoveCategory::Super;
    m.startup = 5;
    m.active = 8;
    m.recovery = 40;
    m.requiresGroundedSelf = true;
    m.requiresWallSplatOpponent = true;
    m.meterCost = 100;
    m.cancelFromMoves = {id236H};
    m.cancelFromCategories = {MoveCategory::Special};
    m.difficulty = 3;
    m.hitboxes.push_back({5, 12, Rect{20, 0, 260, 110}});
    m.onHit.damage = 180;
    m.onHit.hitstun = 0;
    m.onHit.hitstop = 8;
    m.onHit.launchVelocity = {0.0f, 0.0f};
    m.onHit.meterCost = 100;
    m.onHit.scalingStep = 0;
    m.onHit.juggleCost = 0;
    m.onHit.causesKnockdown = true;
    return m;
}

inline MoveDatabase makeExampleMoveDatabase() {
    MoveDatabase db;
    const MoveId id2M = db.add(make2M());
    const MoveId id5H = db.add(make5H(id2M));
    const MoveId id2H = db.add(make2H(id5H));
    const MoveId idJM = db.add(makeJM(id2H));
    const MoveId idJH = db.add(makeJH(idJM));
    const MoveId id236H = db.add(make236H());
    (void)idJH;
    db.add(makeSuper1(id236H));
    return db;
}

inline FrameState makeInitialFrameState() {
    FrameState s;
    // Start close to the right wall so 236H can produce a wall splat.
    s.self.pos = {760.0f, 0.0f};
    s.self.vel = {0.0f, 0.0f};
    s.self.body = BodyState::Grounded;
    s.self.phase = ActionPhase::Idle;
    s.self.facingRight = true;
    s.self.hurtboxLocal = Rect{-15, 0, 30, 80};

    s.opp.pos = {820.0f, 0.0f};
    s.opp.vel = {0.0f, 0.0f};
    s.opp.body = BodyState::Grounded;
    s.opp.phase = ActionPhase::Hitstun;
    s.opp.hitstunRemaining = 0;
    s.opp.facingRight = false;
    s.opp.hurtboxLocal = Rect{-15, 0, 30, 80};

    s.meter = 100;
    s.scalingBucket = 0;
    s.juggle = 0;
    s.wallBounceUsed = false;
    s.groundBounceUsed = false;
    s.opponentWallSplat = false;
    s.lastMove = MOVE_NONE;
    return s;
}

} // namespace combolens

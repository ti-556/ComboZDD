#pragma once

#include "combolens/core/Rect.hpp"

namespace combolens {

enum class BodyState : std::uint8_t {
    Grounded,
    Airborne,
    Knockdown
};

enum class ActionPhase : std::uint8_t {
    Idle,
    Startup,
    Active,
    Recovery,
    Hitstun,
    Knockdown
};

enum class MoveCategory : std::uint8_t {
    Normal,
    Launcher,
    AirNormal,
    Special,
    Super
};

enum class CancelType : std::uint8_t {
    None    = 0,
    Normal  = 1 << 0,
    Special = 1 << 1,
    Super   = 1 << 2,
    Jump    = 1 << 3
};

using CancelMask = std::uint8_t;

inline CancelMask cancelBit(CancelType type) {
    return static_cast<CancelMask>(type);
}

inline bool hasCancel(CancelMask mask, CancelType type) {
    return (mask & cancelBit(type)) != 0;
}

inline CancelMask operator|(CancelType a, CancelType b) {
    return static_cast<CancelMask>(cancelBit(a) | cancelBit(b));
}

struct HitboxFrame {
    int startFrame = 0;
    int endFrame = 0;
    Rect boxLocal;
};

struct CancelWindow {
    int startFrame = 0;
    int endFrame = 0;
    CancelMask mask = 0;
};

struct HitEffect {
    int damage = 0;
    int hitstun = 0;
    int hitstop = 0;
    Vec2 launchVelocity;

    int meterGain = 0;
    int meterCost = 0;
    int scalingStep = 0;
    int juggleCost = 0;

    bool causesWallSplat = false;
    bool causesGroundBounce = false;
    bool causesKnockdown = false;

    // Simplified self-state effects for v1 hybrid search.
    bool selfBecomesAirborne = false;
    bool selfBecomesGrounded = false;

    bool usesWallBounce = false;
    bool usesGroundBounce = false;
};

struct MoveDef {
    MoveId id = MOVE_NONE;
    std::string name;
    std::string label;
    MoveCategory category = MoveCategory::Normal;

    int startup = 0;
    int active = 0;
    int recovery = 0;

    std::vector<HitboxFrame> hitboxes;
    std::vector<CancelWindow> cancelWindows;
    HitEffect onHit;

    // Coarse requirements used before running frame simulation.
    bool canStartCombo = false;
    bool requiresGroundedSelf = false;
    bool requiresAirborneSelf = false;
    bool requiresGroundedOpponent = false;
    bool requiresAirborneOpponent = false;
    bool requiresWallSplatOpponent = false;

    int meterCost = 0;

    std::vector<MoveId> cancelFromMoves;
    std::vector<MoveCategory> cancelFromCategories;

    int difficulty = 1;
};

class MoveDatabase {
public:
    MoveId add(MoveDef move) {
        if (move.id == MOVE_NONE || move.id == MOVE_END) {
            move.id = nextId_++;
        } else {
            nextId_ = std::max<MoveId>(nextId_, static_cast<MoveId>(move.id + 1));
        }

        if (move.id >= byId_.size()) {
            byId_.resize(move.id + 1);
        }
        byId_[move.id] = move;
        nameToId_[move.name] = move.id;
        moves_.push_back(move);
        return move.id;
    }

    const MoveDef& byId(MoveId id) const {
        if (id >= byId_.size() || !byId_[id].has_value()) {
            throw std::runtime_error("Invalid MoveId");
        }
        return *byId_[id];
    }

    MoveId idByName(const std::string& name) const {
        auto it = nameToId_.find(name);
        if (it == nameToId_.end()) {
            throw std::runtime_error("Unknown move name: " + name);
        }
        return it->second;
    }

    const std::vector<MoveDef>& moves() const {
        return moves_;
    }

private:
    MoveId nextId_ = 2;
    std::vector<MoveDef> moves_;
    std::vector<std::optional<MoveDef>> byId_;
    std::unordered_map<std::string, MoveId> nameToId_;
};

inline bool moveInList(MoveId id, const std::vector<MoveId>& list) {
    return std::find(list.begin(), list.end(), id) != list.end();
}

inline bool categoryInList(MoveCategory category, const std::vector<MoveCategory>& list) {
    return std::find(list.begin(), list.end(), category) != list.end();
}

} // namespace combolens

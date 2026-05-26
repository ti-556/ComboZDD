#pragma once

#include "combolens/core/Types.hpp"

namespace combolens {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

inline bool overlaps(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

inline Rect flippedLocalRect(const Rect& local) {
    // If a hitbox is authored for facing-right, flipping mirrors it around origin.
    return Rect{-local.x - local.w, local.y, local.w, local.h};
}

} // namespace combolens

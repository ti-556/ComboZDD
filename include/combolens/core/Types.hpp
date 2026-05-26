#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace combolens {

using MoveId = std::uint16_t;

constexpr MoveId MOVE_NONE = 0;
constexpr MoveId MOVE_END  = 1;

constexpr int MAX_COMBO_LEN = 8;
constexpr int MAX_MOVE_ID = 64;

constexpr int STAGE_LEFT = 0;
constexpr int STAGE_RIGHT = 1000;
constexpr int FLOOR_Y = 0;

constexpr int MAX_METER = 300;
constexpr int METER_BUCKET_SIZE = 50;
constexpr int MAX_METER_BUCKET = 6;
constexpr int MAX_SCALING_BUCKET = 8; // 0=100%, 8=20%
constexpr int MAX_JUGGLE = 10;

inline int clampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(v, hi));
}

inline std::uint8_t clampU8(int v, int lo, int hi) {
    return static_cast<std::uint8_t>(clampInt(v, lo, hi));
}

inline float clampFloat(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

} // namespace combolens

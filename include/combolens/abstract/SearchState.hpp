#pragma once

#include "combolens/frame/FrameState.hpp"

namespace combolens {

enum class SearchSelfState : std::uint8_t {
    Grounded,
    Airborne
};

enum class SearchOppState : std::uint8_t {
    Grounded,
    Airborne,
    WallSplat,
    GroundBounce,
    Knockdown,
    Neutral
};

struct SearchState {
    SearchSelfState self = SearchSelfState::Grounded;
    SearchOppState opp = SearchOppState::Grounded;

    std::uint8_t selfActionableIn = 0;
    std::uint8_t oppHitstunRemaining = 0;
    std::uint8_t cancelWindowRemaining = 0;
    CancelMask availableCancels = 0;

    std::uint8_t height = 0;
    std::uint8_t distance = 0;
    std::uint8_t wallDist = 0;

    std::uint8_t meter = 0;
    std::uint8_t scaling = 0;
    std::uint8_t juggle = 0;

    bool wallBounceUsed = false;
    bool groundBounceUsed = false;

    MoveId lastMove = MOVE_NONE;

    bool operator==(const SearchState& other) const = default;
};

struct SearchStateHash {
    std::size_t operator()(const SearchState& s) const {
        std::size_t h = 1469598103934665603ull;
        auto mix = [&](std::size_t v) {
            h ^= v;
            h *= 1099511628211ull;
        };

        mix(static_cast<std::size_t>(s.self));
        mix(static_cast<std::size_t>(s.opp));
        mix(s.selfActionableIn);
        mix(s.oppHitstunRemaining);
        mix(s.cancelWindowRemaining);
        mix(s.availableCancels);
        mix(s.height);
        mix(s.distance);
        mix(s.wallDist);
        mix(s.meter);
        mix(s.scaling);
        mix(s.juggle);
        mix(s.wallBounceUsed);
        mix(s.groundBounceUsed);
        mix(s.lastMove);
        return h;
    }
};

struct SearchNode {
    SearchState abstract;
    FrameState representative;
};

} // namespace combolens

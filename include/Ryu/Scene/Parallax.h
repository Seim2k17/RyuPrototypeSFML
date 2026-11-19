/**
 * Per-layer parallax factors of a level.
 *
 * A parallax factor tells the renderer how fast a scene layer moves
 * relative to the camera: 1.0 = glued to the world (the walk plane),
 * < 1 = slower (background), > 1 = faster (foreground). Stored in the
 * level JSON root ("parallax": [bg, g1, fg]) and applied per level.
 *
 * @author: Simon Nguyen
 * @date: 02/02/2025
 */

#pragma once

#include <array>

namespace RyuLevel
{

/// parallax factor per renderer scene layer (Background, Ground1,
/// Foreground); 1.0 = glued to the world
using ParallaxFactors = std::array<float, 3>;
inline constexpr ParallaxFactors kParallaxDefaults = { 0.5f, 1.f, 1.2f };

} // namespace RyuLevel

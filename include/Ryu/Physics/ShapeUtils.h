#pragma once

// Safe Box2D box-collider builders.
//
// Box2D v3 asserts `halfWidth > 0 && halfHeight > 0` inside b2MakeBox
// (box2d-src/src/geometry.c). The assert is compiled in whenever NDEBUG is
// NOT defined (Debug or empty CMAKE_BUILD_TYPE builds) and traps with
// SIGILL; in Release builds it is compiled out and the call silently
// produces an inverted, degenerate polygon. Any object whose size is smaller
// than twice the corner-rounding radius hits exactly that: subtracting the
// rounding from the half-extents yields a negative value (seen with a 4px
// tall button and the 0.07m/~2.2px tile rounding -> halfHeight = -0.0075m).
//
// These helpers clamp the extents/radius so a valid collider can always be
// built in EVERY build type, and print a warning when clamping kicked in.

#include <box2d/box2d.h>

#include <algorithm>
#include <fmt/core.h>

namespace RyuPhysics
{

/// Smallest allowed half-extent (meters): 1mm. Keeps b2MakeBox's
/// `half > 0` requirement satisfied even for zero/negative input sizes.
inline constexpr float MIN_BOX_HALF_EXTENT_M = 0.001f;

/// Rounded box built from the FULL size in meters. The inner box is inset by
/// the radius because Box2D expands a polygon outward by its radius, so the
/// collider's outer bounds stay exactly `sizeXM x sizeYM` (halfX/halfY).
/// The old code passed `half - rounding` straight to b2MakeRoundedBox, which
/// asserts `half > 0` - that crashes whenever rounding >= min(halfX, halfY).
/// Here the radius is clamped to just below that, so previously-valid sizes
/// keep their exact old polygon, and only genuinely too-small objects (which
/// used to SIGILL in non-NDEBUG builds) get a reduced radius + a warning.
inline b2Polygon makeSafeRoundedBox(float sizeXM, float sizeYM, float roundingM,
                                    const char* what = "collider")
{
    const float halfX = std::max(sizeXM * 0.5f, MIN_BOX_HALF_EXTENT_M);
    const float halfY = std::max(sizeYM * 0.5f, MIN_BOX_HALF_EXTENT_M);
    const float radius = std::clamp(roundingM, 0.0f, std::min(halfX, halfY) * 0.9f);
    if (radius != roundingM || halfX != sizeXM * 0.5f || halfY != sizeYM * 0.5f)
    {
        fmt::print("Physics: '{}' size {:.4f}x{:.4f} m too small for {:.4f} m "
                   "rounding - radius clamped to {:.4f} m (collider stays "
                   "valid in all build types).\n",
                   what, sizeXM, sizeYM, roundingM, radius);
    }
    return b2MakeRoundedBox(halfX - radius, halfY - radius, radius);
}

/// Sharp-cornered (radius 0) box from the FULL size in meters, with the same
/// safeguard against non-positive sizes.
inline b2Polygon makeSafeBox(float sizeXM, float sizeYM, const char* what = "collider")
{
    const float halfX = std::max(sizeXM * 0.5f, MIN_BOX_HALF_EXTENT_M);
    const float halfY = std::max(sizeYM * 0.5f, MIN_BOX_HALF_EXTENT_M);
    if (halfX != sizeXM * 0.5f || halfY != sizeYM * 0.5f)
    {
        fmt::print("Physics: '{}' size {:.4f}x{:.4f} m is not positive - "
                   "clamped to {:.4f}x{:.4f} m.\n",
                   what, sizeXM, sizeYM, halfX * 2.0f, halfY * 2.0f);
    }
    return b2MakeBox(halfX, halfY);
}

} // namespace RyuPhysics

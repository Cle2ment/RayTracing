#pragma once

// ─── Mathematical Constants ───
constexpr float kPi = 3.14159265358979323846f;

// ─── Epsilon Constants ───
constexpr float kSelfIntersectionEpsilon  = 0.0001f; // normal offset for ray self-intersection avoidance
constexpr float kDenominatorEpsilon       = 0.0001f; // GGX_G1 division guard
constexpr float kRoughnessMin             = 0.001f;  // minimum roughness for microfacet stability
constexpr float kNdotMin                  = 0.001f;  // minimum dot(N,*) for microfacet stability
constexpr float kRussianRouletteThreshold = 0.001f;  // path termination probability threshold
constexpr float kSpecDenominatorEps       = 0.001f;  // specular BRDF denominator guard
constexpr float kInSphereEpsilon          = 0.000001f; // rejection threshold for RandomInUnitSphere (1e-6)

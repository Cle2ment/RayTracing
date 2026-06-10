#include <catch_amalgamated.hpp>
#include <glm/glm.hpp>
#include "PathTracerCore.h"
#include "Constants.h"

using namespace PathTracerCore;

constexpr float kTol = 0.0001f;

TEST_CASE("FresnelSchlick normal incidence returns F0", "[Fresnel]")
{
    // At θ=0 (cosTheta=1), Fresnel = F0
    glm::vec3 F0(0.04f);
    auto result = FresnelSchlick(1.0f, F0);
    REQUIRE_THAT(result.r, Catch::Matchers::WithinAbs(F0.r, kTol));
    REQUIRE_THAT(result.g, Catch::Matchers::WithinAbs(F0.g, kTol));
    REQUIRE_THAT(result.b, Catch::Matchers::WithinAbs(F0.b, kTol));
}

TEST_CASE("FresnelSchlick grazing returns 1", "[Fresnel]")
{
    // At θ=90° (cosTheta=0), Fresnel → 1 (total reflection)
    glm::vec3 F0(0.04f);
    auto result = FresnelSchlick(0.0f, F0);
    REQUIRE_THAT(result.r, Catch::Matchers::WithinAbs(1.0f, kTol));
    REQUIRE_THAT(result.g, Catch::Matchers::WithinAbs(1.0f, kTol));
    REQUIRE_THAT(result.b, Catch::Matchers::WithinAbs(1.0f, kTol));
}

TEST_CASE("FresnelSchlick metal reflects more", "[Fresnel]")
{
    // Higher F0 (metal) → higher reflectance at all angles
    glm::vec3 dielectric(0.04f);
    glm::vec3 metal(0.9f);
    auto rDiel = FresnelSchlick(0.5f, dielectric);
    auto rMetal = FresnelSchlick(0.5f, metal);
    REQUIRE(rMetal.r > rDiel.r);
}

TEST_CASE("FresnelSchlick monotonic", "[Fresnel]")
{
    // Reflectance increases monotonically as cosTheta decreases
    glm::vec3 F0(0.5f);
    REQUIRE(FresnelSchlick(1.0f, F0).r < FresnelSchlick(0.5f, F0).r);
    REQUIRE(FresnelSchlick(0.5f, F0).r < FresnelSchlick(0.0f, F0).r);
}

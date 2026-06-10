#include <catch_amalgamated.hpp>
#include <glm/glm.hpp>
#include "PathTracerCore.h"
#include "Constants.h"

using namespace PathTracerCore;

constexpr float kFloatTolerance = 0.0001f;

TEST_CASE("GGX_D normal incidence", "[GGX]")
{
    float a = 0.5f;
    float expected = 1.0f / (kPi * a * a);
    REQUIRE_THAT(GGX_D(1.0f, a), Catch::Matchers::WithinAbs(expected, kFloatTolerance));
}

TEST_CASE("GGX_D grazing angle", "[GGX]")
{
    float result = GGX_D(0.01f, 0.5f);
    REQUIRE(result > 0.0f);
    REQUIRE(result < 1.0f);
}

TEST_CASE("GGX_D zero roughness", "[GGX]")
{
    float a = 0.001f;
    float atPeak = GGX_D(1.0f, a);
    float offPeak = GGX_D(0.5f, a);
    REQUIRE(atPeak > 0.0f);
    REQUIRE(offPeak > 0.0f);
    REQUIRE(atPeak > offPeak * 10.0f);
}

TEST_CASE("GGX_G1 normal incidence", "[GGX]")
{
    REQUIRE_THAT(GGX_G1(1.0f, 0.5f), Catch::Matchers::WithinAbs(1.0f, kFloatTolerance));
}

TEST_CASE("GGX_G1 grazing angle", "[GGX]")
{
    float result = GGX_G1(0.001f, 0.5f);
    REQUIRE(result > 0.0f);
    REQUIRE(result < 0.1f);
}

TEST_CASE("GGX_G is product of G1s", "[GGX]")
{
    float a = 0.5f;
    float NdotL = 0.8f;
    float NdotV = 0.6f;
    float expected = GGX_G1(NdotL, a) * GGX_G1(NdotV, a);
    REQUIRE_THAT(GGX_G(NdotL, NdotV, a), Catch::Matchers::WithinAbs(expected, kFloatTolerance));
}

TEST_CASE("GGX_G symmetry", "[GGX]")
{
    float a = 0.3f;
    REQUIRE_THAT(GGX_G(0.7f, 0.5f, a), Catch::Matchers::WithinAbs(GGX_G(0.5f, 0.7f, a), kFloatTolerance));
}

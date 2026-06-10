#include <catch_amalgamated.hpp>
#include <glm/glm.hpp>
#include "PathTracerCore.h"
#include "Constants.h"

using namespace PathTracerCore;

constexpr float kTol = 0.0001f;

TEST_CASE("RandomFloat range [0,1)", "[Random]")
{
    uint32_t seed = 42u;
    for (int i = 0; i < 100; i++)
    {
        float r = RandomFloat(seed);
        REQUIRE(r >= 0.0f);
        REQUIRE(r < 1.0f);
    }
}

TEST_CASE("RandomFloat deterministic sequence", "[Random]")
{
    // Same seed → same sequence
    uint32_t s1 = 42u, s2 = 42u;
    for (int i = 0; i < 10; i++)
    {
        REQUIRE(RandomFloat(s1) == RandomFloat(s2));
    }
}

TEST_CASE("RandomFloat different seeds diverge", "[Random]")
{
    uint32_t s1 = 42u, s2 = 43u;
    REQUIRE(RandomFloat(s1) != RandomFloat(s2));
}

TEST_CASE("RandomFloat mutates seed", "[Random]")
{
    uint32_t seed = 42u;
    uint32_t before = seed;
    RandomFloat(seed);
    REQUIRE(seed != before); // seed must be updated
}

TEST_CASE("InUnitSphere produces unit vector", "[Random]")
{
    uint32_t seed = 42u;
    for (int i = 0; i < 20; i++)
    {
        glm::vec3 v = InUnitSphere(seed);
        float len = glm::length(v);
        REQUIRE_THAT(len, Catch::Matchers::WithinAbs(1.0f, kTol));
    }
}

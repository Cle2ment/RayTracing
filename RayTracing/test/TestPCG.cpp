#include <catch_amalgamated.hpp>
#include "PathTracerCore.h"

using namespace PathTracerCore;

TEST_CASE("PCG_Hash is deterministic", "[PCG]")
{
    REQUIRE(PCG_Hash(0u) == PCG_Hash(0u));
    REQUIRE(PCG_Hash(42u) == PCG_Hash(42u));
    REQUIRE(PCG_Hash(UINT32_MAX) == PCG_Hash(UINT32_MAX));
}

TEST_CASE("PCG_Hash avalanche", "[PCG]")
{
    // Small input change should produce very different output
    REQUIRE(PCG_Hash(0u) != PCG_Hash(1u));
    REQUIRE(PCG_Hash(100u) != PCG_Hash(101u));
}

TEST_CASE("PCG_Hash non-zero", "[PCG]")
{
    REQUIRE(PCG_Hash(0u) != 0u);
    REQUIRE(PCG_Hash(1u) != 0u);
    REQUIRE(PCG_Hash(42u) != 0u);
}

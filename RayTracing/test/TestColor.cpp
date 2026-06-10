#include <catch_amalgamated.hpp>
#include <glm/glm.hpp>
#include "PathTracerCore.h"

using namespace PathTracerCore;

TEST_CASE("ConvertToRGBA black", "[Color]")
{
    // Black with alpha=1 → ABGR: 0xFF000000
    REQUIRE(ConvertToRGBA(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) == 0xFF000000u);
}

TEST_CASE("ConvertToRGBA white", "[Color]")
{
    // White with alpha=1 → ABGR: 0xFFFFFFFF
    REQUIRE(ConvertToRGBA(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)) == 0xFFFFFFFFu);
}

TEST_CASE("ConvertToRGBA red", "[Color]")
{
    // Pure red with alpha=1 → ABGR: 0xFF0000FF
    REQUIRE(ConvertToRGBA(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)) == 0xFF0000FFu);
}

TEST_CASE("ConvertToRGBA alpha is MSB", "[Color]")
{
    // Verify alpha is in highest byte
    uint32_t result = ConvertToRGBA(glm::vec4(1.0f, 1.0f, 1.0f, 0.5f));
    // Alpha=0.5 → ~128, should be in top byte
    REQUIRE((result >> 24) == 127u);
}

TEST_CASE("ConvertToRGBA wraps via uint8_t modulo", "[Color]")
{
    // 2.0 * 255 = 510 → uint8_t wraps: 510 % 256 = 254
    uint32_t overbright = ConvertToRGBA(glm::vec4(2.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(overbright == 0xFF0000FEu);
}

#include <catch_amalgamated.hpp>
#include <glm/glm.hpp>
#include "PathTracerCore.h"

using namespace PathTracerCore;

constexpr float kTol = 0.0001f;

static void CheckONBOrthonormal(glm::vec3 u, glm::vec3 v, glm::vec3 w)
{
    // All axes are unit length
    REQUIRE_THAT(glm::length(u), Catch::Matchers::WithinAbs(1.0f, kTol));
    REQUIRE_THAT(glm::length(v), Catch::Matchers::WithinAbs(1.0f, kTol));
    REQUIRE_THAT(glm::length(w), Catch::Matchers::WithinAbs(1.0f, kTol));
    // Mutually orthogonal
    REQUIRE_THAT(glm::dot(u, v), Catch::Matchers::WithinAbs(0.0f, kTol));
    REQUIRE_THAT(glm::dot(v, w), Catch::Matchers::WithinAbs(0.0f, kTol));
    REQUIRE_THAT(glm::dot(w, u), Catch::Matchers::WithinAbs(0.0f, kTol));
}

TEST_CASE("BuildONB Z axis", "[ONB]")
{
    glm::vec3 u, v, w;
    glm::vec3 n(0, 0, 1);
    BuildONB(n, u, v, w);
    REQUIRE_THAT(w.z, Catch::Matchers::WithinAbs(1.0f, kTol));
    CheckONBOrthonormal(u, v, w);
}

TEST_CASE("BuildONB X axis", "[ONB]")
{
    glm::vec3 u, v, w;
    glm::vec3 n(1, 0, 0);
    BuildONB(n, u, v, w);
    REQUIRE_THAT(w.x, Catch::Matchers::WithinAbs(1.0f, kTol));
    CheckONBOrthonormal(u, v, w);
}

TEST_CASE("BuildONB diagonal", "[ONB]")
{
    glm::vec3 u, v, w;
    glm::vec3 n = glm::normalize(glm::vec3(1, 1, 1));
    BuildONB(n, u, v, w);
    REQUIRE_THAT(glm::dot(w, n), Catch::Matchers::WithinAbs(1.0f, kTol));
    CheckONBOrthonormal(u, v, w);
}

TEST_CASE("BuildONB near-negative Z", "[ONB]")
{
    // This is the branch where w.z ≤ 0 (sign = -1)
    glm::vec3 u, v, w;
    glm::vec3 n(0, 0, -1);
    BuildONB(n, u, v, w);
    REQUIRE_THAT(w.z, Catch::Matchers::WithinAbs(-1.0f, kTol));
    CheckONBOrthonormal(u, v, w);
}

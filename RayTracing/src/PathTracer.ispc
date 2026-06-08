// ──────────────────────────────────────────────
// PathTracer.ispc — ISPC-accelerated CPU path tracer
// Compile: ispc --target=avx2,avx512skx-i32x16 --arch=x86-64
// ──────────────────────────────────────────────

// ── Named Constants ──

static const float kPi = 3.14159265358979323846f;
static const float kSelfIntersectionEpsilon = 0.0001f;
static const float kDenominatorEpsilon = 0.0001f;
static const float kRoughnessMin = 0.001f;
static const float kNdotMin = 0.001f;
static const float kRussianRouletteThreshold = 0.001f;
static const float kSpecDenominatorEps = 0.001f;
static const float kInSphereEpsilon = 1e-6f;

// ── RNG: PCG Hash (matches Renderer.cpp Utils::PCG_Hash exactly) ──
// Use unsigned int32: PCG hash relies on unsigned overflow behavior (well-defined).
// ISPC's unsigned int32→float conversion is efficient on modern targets (AVX2/AVX512).

static inline float RandomFloat(varying unsigned int32& seed) {
    varying unsigned int32 state = seed * 747796405u + 2891336453u;
    varying unsigned int32 word  = ((state >> ((state >> 28) + 4)) ^ state) * 277803737u;
    seed = (word >> 22) ^ word;
    return (float)(seed & 0x7fffffffu) * 2.3283064365386963e-10f;  // 1.0f / 2^31
}

static inline void RandomInUnitSphere(
    varying float result[3],
    varying unsigned int32& seed
) {
    varying float x, y, z, lenSq;
    // Rejection sampling in unit sphere
    do {
        x = RandomFloat(seed) * 2.0f - 1.0f;
        y = RandomFloat(seed) * 2.0f - 1.0f;
        z = RandomFloat(seed) * 2.0f - 1.0f;
        lenSq = x * x + y * y + z * z;
    } while (lenSq > 1.0f || lenSq < kInSphereEpsilon);

    varying float invLen = rsqrt(lenSq);
    result[0] = x * invLen;
    result[1] = y * invLen;
    result[2] = z * invLen;
}

// ── Helper: 3D vector dot product ──

static inline float dot3(float ax, float ay, float az, float bx, float by, float bz) {
    return ax * bx + ay * by + az * bz;
}

// ── Helper: normalize 3D vector (in-place) ──

static inline void normalize3(varying float v[3]) {
    varying float lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (lenSq > 0.0f) {
        varying float invLen = rsqrt(lenSq);
        v[0] *= invLen;
        v[1] *= invLen;
        v[2] *= invLen;
    }
}

// ── GGX Microfacet BRDF ──

static inline void FresnelSchlick3(float cosTheta, varying float F0[3], varying float out[3]) {
    varying float t = max(1.0f - cosTheta, 0.0f);
    varying float t5 = t * t * t * t * t;
    out[0] = F0[0] + (1.0f - F0[0]) * t5;
    out[1] = F0[1] + (1.0f - F0[1]) * t5;
    out[2] = F0[2] + (1.0f - F0[2]) * t5;
}

static inline float GGX_D(float NdotH, float a) {
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (kPi * d * d);
}

static inline float GGX_G1(float NdotV, float a) {
    float a2 = a * a;
    float denom = NdotV + sqrt(NdotV * NdotV * (1.0f - a2) + a2);
    return 2.0f * NdotV / max(denom, kDenominatorEpsilon);
}

static inline float GGX_G(float NdotL, float NdotV, float a) {
    return GGX_G1(NdotL, a) * GGX_G1(NdotV, a);
}

static inline void SampleGGX_VNDF(
    varying float Vx, varying float Vy, varying float Vz,
    float a, varying float r1, varying float r2,
    varying float out[3]
) {
    // Stretch V
    varying float Vhx = a * Vx, Vhy = a * Vy, Vhz = Vz;
    varying float vhLen = sqrt(Vhx*Vhx + Vhy*Vhy + Vhz*Vhz);
    Vhx /= vhLen; Vhy /= vhLen; Vhz /= vhLen;

    // T1 = normalize(cross(up, Vh))
    varying float T1x, T1y, T1z;
    if (Vhz < 0.9999f) {
        T1x = -Vhy; T1y = Vhx; T1z = 0.0f;
        varying float t1Len = sqrt(T1x*T1x + T1y*T1y);
        T1x /= t1Len; T1y /= t1Len;
    } else {
        T1x = 1.0f; T1y = 0.0f; T1z = 0.0f;
    }

    // T2 = cross(Vh, T1)
    varying float T2x = Vhy*T1z - Vhz*T1y;
    varying float T2y = Vhz*T1x - Vhx*T1z;
    varying float T2z = Vhx*T1y - Vhy*T1x;

    varying float r = sqrt(r1);
    varying float phi = 2.0f * kPi * r2;
    varying float t1 = r * cos(phi);
    varying float t2 = r * sin(phi);
    varying float s = 0.5f * (1.0f + Vhz);
    t2 = (1.0f - s) * sqrt(max(1.0f - t1*t1, 0.0f)) + s * t2;

    varying float nhx = t1*T1x + t2*T2x + sqrt(max(1.0f - t1*t1 - t2*t2, 0.0f)) * Vhx;
    varying float nhy = t1*T1y + t2*T2y + sqrt(max(1.0f - t1*t1 - t2*t2, 0.0f)) * Vhy;
    varying float nhz = t1*T1z + t2*T2z + sqrt(max(1.0f - t1*t1 - t2*t2, 0.0f)) * Vhz;

    // Unstretch
    out[0] = a * nhx;
    out[1] = a * nhy;
    out[2] = max(0.0f, nhz);
    normalize3(out);
}

// ── Sphere Intersection (matches Renderer.cpp TraceRay exactly) ──
// Returns: hitDistance (>0 = hit, <0 = miss), and sets closestSphere index
// The quadratic: a*t^2 + b*t + c = 0  where a = dot(dir,dir) ≈ 1

static inline float TraceRay(
    varying float rayOrigX, varying float rayOrigY, varying float rayOrigZ,
    varying float rayDirX,  varying float rayDirY,  varying float rayDirZ,
    uniform float sphPosX[],  uniform float sphPosY[],  uniform float sphPosZ[],
    uniform float sphRadius[], uniform int   sphMatIdx[],
    uniform int sphereCount,
    varying int& closestSphere
) {
    varying float hitDistance = 1e30f;
    closestSphere = -1;

    // a = dot(dir, dir) — same for all spheres, precompute once
    varying float a = rayDirX * rayDirX + rayDirY * rayDirY + rayDirZ * rayDirZ;

    for (uniform int i = 0; i < sphereCount; i++) {
        // origin = rayOrigin - spherePosition
        varying float ox = rayOrigX - sphPosX[i];
        varying float oy = rayOrigY - sphPosY[i];
        varying float oz = rayOrigZ - sphPosZ[i];

        varying float b = 2.0f * (ox * rayDirX + oy * rayDirY + oz * rayDirZ);
        varying float c = ox * ox + oy * oy + oz * oz - sphRadius[i] * sphRadius[i];

        varying float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f)
            continue;

        varying float closestT = (-b - sqrt(discriminant)) / (2.0f * a);
        if (closestT > 0.0f && closestT < hitDistance) {
            hitDistance = closestT;
            closestSphere = i;
        }
    }

    return hitDistance;
}

// ── Main Entry Point: Render all pixels ──

export void ISPCRenderPixels(
    // Camera
    uniform float camPosX, uniform float camPosY, uniform float camPosZ,
    uniform float rayDirX[], uniform float rayDirY[], uniform float rayDirZ[],
    // Spheres (SoA layout)
    uniform float sphPosX[],  uniform float sphPosY[],  uniform float sphPosZ[],
    uniform float sphRadius[], uniform int   sphMatIdx[],
    // Materials (SoA layout)
    uniform float matAlbedoR[], uniform float matAlbedoG[], uniform float matAlbedoB[],
    uniform float matRoughness[], uniform float matMetallic[],
    uniform float matEmissionR[], uniform float matEmissionG[], uniform float matEmissionB[],
    uniform float matEmissionPower[],
    // Output
    uniform float outputR[], uniform float outputG[], uniform float outputB[], uniform float outputA[],
    // Counts & settings
    uniform int pixelCount, uniform int sphereCount,
    uniform int frameIndex, uniform int maxBounces
) {
    // ISPC foreach: each SIMD lane gets a different pixel index
    foreach (i = 0 ... pixelCount) {
        // ── Initialize ray ──
        varying float rOrigX = camPosX;
        varying float rOrigY = camPosY;
        varying float rOrigZ = camPosZ;
        varying float rDirX = rayDirX[i];
        varying float rDirY = rayDirY[i];
        varying float rDirZ = rayDirZ[i];

        // ── Path tracing state ──
        varying float lightR = 0.0f, lightG = 0.0f, lightB = 0.0f;
        varying float contribR = 1.0f, contribG = 1.0f, contribB = 1.0f;

        // ── RNG seed (per-pixel, per-frame) ──
        varying unsigned int32 seed = (unsigned int32)i;
        seed *= (unsigned int32)frameIndex;

        for (uniform int bounce = 0; bounce < maxBounces; bounce++) {
            seed += (unsigned int32)bounce;

            // Trace ray against all spheres
            varying int hitObj;
            varying float hitDist = TraceRay(
                rOrigX, rOrigY, rOrigZ,
                rDirX, rDirY, rDirZ,
                sphPosX, sphPosY, sphPosZ,
                sphRadius, sphMatIdx,
                sphereCount,
                hitObj
            );

            // Miss — terminate path
            if (hitDist < 0.0f)
                break;

            // ── Load hit data once (avoid repeated gathers on varying indices) ──
            varying float hsx = sphPosX[hitObj];
            varying float hsy = sphPosY[hitObj];
            varying float hsz = sphPosZ[hitObj];
            varying int   hmi = sphMatIdx[hitObj];

            // ── Compute hit position & normal using loaded locals ──
            varying float hitPosX = rOrigX - hsx + rDirX * hitDist;
            varying float hitPosY = rOrigY - hsy + rDirY * hitDist;
            varying float hitPosZ = rOrigZ - hsz + rDirZ * hitDist;

            varying float normalLen = sqrt(hitPosX * hitPosX + hitPosY * hitPosY + hitPosZ * hitPosZ);
            varying float normX = hitPosX, normY = hitPosY, normZ = hitPosZ;
            if (normalLen > 0.0f) {
                varying float invLen = 1.0f / normalLen;
                normX *= invLen;
                normY *= invLen;
                normZ *= invLen;
            }

            // World-space hit position
            hitPosX += hsx;
            hitPosY += hsy;
            hitPosZ += hsz;

            // ── Load material data once ──
            varying float emR = matEmissionR[hmi] * matEmissionPower[hmi];
            varying float emG = matEmissionG[hmi] * matEmissionPower[hmi];
            varying float emB = matEmissionB[hmi] * matEmissionPower[hmi];
            varying float alR = matAlbedoR[hmi];
            varying float alG = matAlbedoG[hmi];
            varying float alB = matAlbedoB[hmi];

            // ── Accumulate emission ──
            lightR += contribR * emR;
            lightG += contribG * emG;
            lightB += contribB * emB;

            // ── Russian roulette (after 3 guaranteed bounces) ──
            // Use luminance (BT.709) for survival probability: fairer than max(channel)
            if (bounce > 2) {
                varying float p = 0.2126f * contribR + 0.7152f * contribG + 0.0722f * contribB;

                if (p < kRussianRouletteThreshold)
                    break;
                if (p < 1.0f && RandomFloat(seed) > p)
                    break;

                varying float invP = 1.0f / p;
                contribR *= invP;
                contribG *= invP;
                contribB *= invP;
            }

            // ── Bounce: new ray origin (offset to avoid self-intersection) ──
            rOrigX = hitPosX + normX * kSelfIntersectionEpsilon;
            rOrigY = hitPosY + normY * kSelfIntersectionEpsilon;
            rOrigZ = hitPosZ + normZ * kSelfIntersectionEpsilon;

            // Load roughness + metallic for GGX
            varying float roughVal = max(matRoughness[hmi], kRoughnessMin);
            varying float metalVal = matMetallic[hmi];
            varying float a = roughVal * roughVal;


            // ── GGX Microfacet BRDF ──
            // Outgoing direction (pointing away from surface)
            varying float woX = -rDirX, woY = -rDirY, woZ = -rDirZ;

            // Fresnel F0
            varying float F0[3];
            F0[0] = 0.04f + (alR - 0.04f) * metalVal;
            F0[1] = 0.04f + (alG - 0.04f) * metalVal;
            F0[2] = 0.04f + (alB - 0.04f) * metalVal;

            // Build ONB from surface normal inlined (Duff et al. 2017)
            varying float ux, uy, uz, vx, vy, vz, wx, wy, wz;
            wx = normX; wy = normY; wz = normZ;
            {
                varying float sign = (wz > 0.0f) ? 1.0f : -1.0f;
                varying float a_val = -1.0f / (sign + wz);
                varying float b = wx * wy * a_val;
                ux = 1.0f + sign * wx * wx * a_val; uy = sign * b;               uz = -sign * wx;
                vx = b;                            vy = sign + wy * wy * a_val;  vz = -wy;
            }

            // Transform wo to local frame where n = (0,0,1)
            varying float localWoX = ux*woX + uy*woY + uz*woZ;
            varying float localWoY = vx*woX + vy*woY + vz*woZ;
            varying float localWoZ = wx*woX + wy*woY + wz*woZ;

            varying float rn1 = RandomFloat(seed), rn2 = RandomFloat(seed);
            varying float localH[3];
            SampleGGX_VNDF(localWoX, localWoY, localWoZ, a, rn1, rn2, localH);
            float NdotH = max(localH[2], kNdotMin);
            varying float WoDotH_signed = localWoX*localH[0] + localWoY*localH[1] + localWoZ*localH[2];

            // Reflect in local frame: wi = 2*(wo·H)*H - wo
            varying float localWiX = 2.0f * WoDotH_signed * localH[0] - localWoX;
            varying float localWiY = 2.0f * WoDotH_signed * localH[1] - localWoY;
            varying float localWiZ = 2.0f * WoDotH_signed * localH[2] - localWoZ;
            float NdotL = max(localWiZ, kNdotMin);
            float NdotV = max(localWoZ, kNdotMin);

            // Transform wi back to world
            varying float wiX = ux*localWiX + vx*localWiY + wx*localWiZ;
            varying float wiY = uy*localWiX + vy*localWiY + wy*localWiZ;
            varying float wiZ = uz*localWiX + vz*localWiY + wz*localWiZ;
            {
                varying float wiLenSq = wiX*wiX + wiY*wiY + wiZ*wiZ;
                if (wiLenSq > 0.0f) {
                    varying float inv = rsqrt(wiLenSq);
                    wiX *= inv; wiY *= inv; wiZ *= inv;
                }
            }

            // WoDotH for Fresnel/PDF (absolute value)
            varying float WoDotH = WoDotH_signed;
            if (WoDotH < 0.0f) WoDotH = -WoDotH;

            float D = GGX_D(NdotH, a);
            float G1_v = GGX_G1(NdotV, a);
            float G = G1_v * GGX_G1(NdotL, a);
            varying float F[3];
            FresnelSchlick3(WoDotH, F0, F);

            varying float specR = D * G * F[0] / (4.0f * NdotL * NdotV + kSpecDenominatorEps);
            varying float specG = D * G * F[1] / (4.0f * NdotL * NdotV + kSpecDenominatorEps);
            varying float specB = D * G * F[2] / (4.0f * NdotL * NdotV + kSpecDenominatorEps);

            varying float kdR = (1.0f - F[0]) * (1.0f - metalVal);
            varying float kdG = (1.0f - F[1]) * (1.0f - metalVal);
            varying float kdB = (1.0f - F[2]) * (1.0f - metalVal);
            varying float diffR = kdR * alR / kPi;
            varying float diffG = kdG * alG / kPi;
            varying float diffB = kdB * alB / kPi;

            // VNDF-correct PDF includes G1 for zero-variance at grazing angles
            varying float pdf = max(G1_v * D / (4.0f * NdotV + kSpecDenominatorEps), kNdotMin);

            varying float bsdfR = (specR + diffR) * NdotL;
            varying float bsdfG = (specG + diffG) * NdotL;
            varying float bsdfB = (specB + diffB) * NdotL;

            contribR *= bsdfR / pdf;
            contribG *= bsdfG / pdf;
            contribB *= bsdfB / pdf;

            rDirX = wiX;
            rDirY = wiY;
            rDirZ = wiZ;
        }

        // ── Write output ──
        outputR[i] = lightR;
        outputG[i] = lightG;
        outputB[i] = lightB;
        outputA[i] = 1.0f;
    }
}

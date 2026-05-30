// ──────────────────────────────────────────────
// PathTracer.ispc — ISPC-accelerated CPU path tracer
// Compile: ispc --target=avx2,avx512skx-i32x16 --arch=x86-64
// ──────────────────────────────────────────────

// ── RNG: PCG Hash (matches Renderer.cpp Utils::PCG_Hash exactly) ──
// Using int32 instead of uint32: ISPC optimizes int32→float conversion
// via native cvtsi2ss, avoiding the slow uint32→float code path.

static inline float RandomFloat(varying int32& seed) {
    varying int32 state = seed * 747796405 + 2891336453;
    varying int32 word  = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
    seed = (word >> 22) ^ word;
    // Convert to [0, 1): mask sign bit, then scale. int32→float is fast.
    return (float)(seed & 0x7fffffff) * 2.3283064365386963e-10f;  // 1.0f / 2^31
}

static inline void RandomInUnitSphere(
    varying float result[3],
    varying int32& seed
) {
    varying float x, y, z, lenSq;
    // Rejection sampling in unit sphere
    do {
        x = RandomFloat(seed) * 2.0f - 1.0f;
        y = RandomFloat(seed) * 2.0f - 1.0f;
        z = RandomFloat(seed) * 2.0f - 1.0f;
        lenSq = x * x + y * y + z * z;
    } while (lenSq > 1.0f || lenSq < 1e-6f);

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
        varying int32 seed = (int32)i;
        seed *= (int32)frameIndex;

        for (uniform int bounce = 0; bounce < maxBounces; bounce++) {
            seed += (int32)bounce;

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

            // ── Accumulate emission (BEFORE albedo attenuation) ──
            lightR += contribR * emR;
            lightG += contribG * emG;
            lightB += contribB * emB;

            // ── Attenuate contribution by albedo ──
            contribR *= alR;
            contribG *= alG;
            contribB *= alB;

            // ── Russian roulette (after 3 guaranteed bounces) ──
            if (bounce > 2) {
                varying float p = contribR;
                if (contribG > p) p = contribG;
                if (contribB > p) p = contribB;

                if (p < 0.001f)
                    break;
                if (p < 1.0f && RandomFloat(seed) > p)
                    break;

                varying float invP = 1.0f / p;
                contribR *= invP;
                contribG *= invP;
                contribB *= invP;
            }

            // ── Bounce: new ray origin (offset to avoid self-intersection) ──
            rOrigX = hitPosX + normX * 0.0001f;
            rOrigY = hitPosY + normY * 0.0001f;
            rOrigZ = hitPosZ + normZ * 0.0001f;

            // ── Diffuse BRDF: sample random direction in hemisphere ──
            varying float randDir[3];
            RandomInUnitSphere(randDir, seed);

            rDirX = normX + randDir[0];
            rDirY = normY + randDir[1];
            rDirZ = normZ + randDir[2];

            // Normalize
            varying float dirLenSq = rDirX * rDirX + rDirY * rDirY + rDirZ * rDirZ;
            if (dirLenSq > 0.0f) {
                varying float invLen = rsqrt(dirLenSq);
                rDirX *= invLen;
                rDirY *= invLen;
                rDirZ *= invLen;
            }
        }

        // ── Write output ──
        outputR[i] = lightR;
        outputG[i] = lightG;
        outputB[i] = lightB;
        outputA[i] = 1.0f;
    }
}

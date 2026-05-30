// ──────────────────────────────────────────────
// PathTracer.ispc — ISPC-accelerated CPU path tracer
// Compile: ispc --target=avx2,avx512skx-i32x16 --arch=x86-64
// ──────────────────────────────────────────────

// ── RNG: PCG Hash (matches Renderer.cpp Utils::PCG_Hash exactly) ──

static inline float RandomFloat(varying uint32& seed) {
    // Inline PCG hash — avoids uniform/varying signature mismatch
    varying uint32 state = seed * 747796405u + 2891336453u;
    varying uint32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    seed = (word >> 22u) ^ word;
    // Convert to float in [0, 1)
    return (float)seed * 2.3283064365386963e-10f;  // 1.0f / 2^32
}

static inline void RandomInUnitSphere(
    varying float result[3],
    varying uint32& seed
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
        varying uint32 seed = (uint32)i;
        seed *= (uint32)frameIndex;

        for (uniform int bounce = 0; bounce < maxBounces; bounce++) {
            seed += (uint32)bounce;

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

            // ── Compute hit position & normal ──
            varying int matIdx = sphMatIdx[hitObj];

            varying float hitPosX = rOrigX - sphPosX[hitObj] + rDirX * hitDist;
            varying float hitPosY = rOrigY - sphPosY[hitObj] + rDirY * hitDist;
            varying float hitPosZ = rOrigZ - sphPosZ[hitObj] + rDirZ * hitDist;

            varying float normalLen = sqrt(hitPosX * hitPosX + hitPosY * hitPosY + hitPosZ * hitPosZ);
            varying float normX = hitPosX, normY = hitPosY, normZ = hitPosZ;
            if (normalLen > 0.0f) {
                varying float invLen = 1.0f / normalLen;
                normX *= invLen;
                normY *= invLen;
                normZ *= invLen;
            }

            // World-space hit position
            hitPosX += sphPosX[hitObj];
            hitPosY += sphPosY[hitObj];
            hitPosZ += sphPosZ[hitObj];

            // ── Accumulate emission (BEFORE albedo attenuation) ──
            // emission = emissionColor * emissionPower
            varying float emR = matEmissionR[matIdx] * matEmissionPower[matIdx];
            varying float emG = matEmissionG[matIdx] * matEmissionPower[matIdx];
            varying float emB = matEmissionB[matIdx] * matEmissionPower[matIdx];

            lightR += contribR * emR;
            lightG += contribG * emG;
            lightB += contribB * emB;

            // ── Attenuate contribution by albedo ──
            contribR *= matAlbedoR[matIdx];
            contribG *= matAlbedoG[matIdx];
            contribB *= matAlbedoB[matIdx];

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

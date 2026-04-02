#pragma once

// GL 3.2 Core shader source strings for XYZPan spatial view.
// All shaders use #version 150 core — requires GL 3.2 context (set in XYZPanGLView constructor).
//
// Two shader pairs:
//   kLine*    — wireframe room / floor grid (simple colored lines with opacity)
//   kSphere*  — listener and source nodes (lit sphere with per-instance color and opacity)

namespace xyzpan {

// ---------------------------------------------------------------------------
// Line shader — used for room wireframe and floor grid
// ---------------------------------------------------------------------------

inline constexpr const char* kLineVertShader = R"(
#version 150 core

in vec3 position;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
}
)";

inline constexpr const char* kLineFragShader = R"(
#version 150 core

uniform vec3  lineColor;
uniform float opacity;

out vec4 outColor;

void main()
{
    outColor = vec4(lineColor, opacity);
}
)";

// ---------------------------------------------------------------------------
// Colored-line shader — per-vertex RGB for room wireframe (axis-colored edges)
// ---------------------------------------------------------------------------

inline constexpr const char* kColorLineVertShader = R"(
#version 150 core

in vec3 position;
in vec3 vertColor;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 vColor;

void main()
{
    vColor      = vertColor;
    gl_Position = projection * view * model * vec4(position, 1.0);
}
)";

inline constexpr const char* kColorLineFragShader = R"(
#version 150 core

in vec3 vColor;

uniform float opacity;

out vec4 outColor;

void main()
{
    outColor = vec4(vColor, opacity);
}
)";

// ---------------------------------------------------------------------------
// Sphere shader — used for listener and source nodes
// Simple diffuse + ambient lighting; normal computed from sphere mesh normals.
// ---------------------------------------------------------------------------

inline constexpr const char* kSphereVertShader = R"(
#version 150 core

in vec3 position;
in vec3 normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 vNormal;
out vec3 vFragPos;

void main()
{
    vec4 worldPos   = model * vec4(position, 1.0);
    vFragPos        = worldPos.xyz;
    // Normal matrix: transpose(inverse(model)) — for uniform scale only, model's mat3 is fine
    vNormal         = mat3(model) * normal;
    gl_Position     = projection * view * worldPos;
}
)";

inline constexpr const char* kSphereFragShader = R"(
#version 150 core

in vec3 vNormal;
in vec3 vFragPos;

uniform vec3  nodeColor;
uniform float opacity;
uniform vec3  lightDir;   // normalized; comes from camera direction
uniform float edgeFade;   // 0.0 = no edge fade (solid), 1.0 = full rim-to-transparent fade

out vec4 outColor;

void main()
{
    vec3 norm     = normalize(vNormal);
    float diff    = max(dot(norm, normalize(lightDir)), 0.0);
    float ambient = 0.35;
    float light   = ambient + (1.0 - ambient) * diff;

    // Edge fade: fragments at the silhouette (normal perpendicular to view)
    // fade to transparent, producing soft-edged spheres for sound waves.
    float rim = abs(dot(norm, normalize(-vFragPos)));
    float fade = mix(1.0, rim, edgeFade);

    outColor = vec4(nodeColor * light, opacity * fade);
}
)";

// ---------------------------------------------------------------------------
// Trail shader — per-vertex alpha for fading position trails
// No model matrix; trail vertices stored in world space.
// ---------------------------------------------------------------------------

inline constexpr const char* kTrailVertShader = R"(
#version 150 core

in vec3 position;
in float alpha;

uniform mat4 projection;
uniform mat4 view;

out float vAlpha;

void main()
{
    vAlpha      = alpha;
    gl_Position = projection * view * vec4(position, 1.0);
}
)";

inline constexpr const char* kTrailFragShader = R"(
#version 150 core

in float vAlpha;

uniform vec3  trailColor;
uniform float baseOpacity;

out vec4 outColor;

void main()
{
    outColor = vec4(trailColor, baseOpacity * vAlpha);
}
)";

// ---------------------------------------------------------------------------
// Text billboard shader — camera-facing textured quads for name labels
// Alpha-only sampling: JUCE rasterises white text on transparent background,
// fragment reads .a and tints with uniform textColor.
// ---------------------------------------------------------------------------

inline constexpr const char* kTextVertShader = R"(
#version 150 core

in vec2 position;   // unit quad [-0.5, 0.5]
in vec2 texCoord;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;      // billboard: translate + camera-aligned rotation + scale

out vec2 vTexCoord;

void main()
{
    vTexCoord   = texCoord;
    gl_Position = projection * view * model * vec4(position, 0.0, 1.0);
}
)";

inline constexpr const char* kTextFragShader = R"(
#version 150 core

in vec2 vTexCoord;

uniform sampler2D textTexture;
uniform vec3  textColor;
uniform float opacity;

out vec4 outColor;

void main()
{
    float a = texture(textTexture, vTexCoord).a;
    outColor = vec4(textColor, a * opacity);
}
)";

// ---------------------------------------------------------------------------
// Skybox shader — fullscreen quad with procedural sky generation
// Reconstructs world-space ray from inverse VP matrix + NDC position.
// Uniform skyType selects variant: 1=Day Clouds, 2=Night Sky, 3=DMT Realm
// ---------------------------------------------------------------------------

inline constexpr const char* kSkyboxVertShader = R"(
#version 150 core

in vec2 position;   // [-1,1] fullscreen quad
out vec2 vNDC;

void main()
{
    vNDC        = position;
    gl_Position = vec4(position, 1.0, 1.0);  // z=1.0 = at far plane
}
)";

inline constexpr const char* kSkyboxFragShader = R"(
#version 150 core

in vec2 vNDC;

uniform mat4  invViewProj;
uniform int   skyType;
uniform float iTime;

out vec4 outColor;

// --- noise utilities ---
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float hash31(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise2D(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

vec3 hsv2rgb(vec3 c) {
    vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0);
    return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

void main()
{
    // Reconstruct world-space ray direction from NDC
    vec4 clip   = vec4(vNDC, 1.0, 1.0);
    vec4 worldH = invViewProj * clip;
    vec3 rayDir = normalize(worldH.xyz / worldH.w);

    // Spherical coords used by multiple variants
    float sTheta = atan(rayDir.z, rayDir.x);
    float sPhi   = asin(clamp(rayDir.y, -1.0, 1.0));
    float h      = rayDir.y;

    vec3 col = vec3(0.0);

    if (skyType == 1) {
        // --- Day Clouds ---
        vec3 zenith  = vec3(0.22, 0.38, 0.75);
        vec3 horizon = vec3(0.55, 0.72, 0.92);
        vec3 nadir   = vec3(0.35, 0.45, 0.60);
        col = h >= 0.0
            ? mix(horizon, zenith, smoothstep(0.0, 0.6, h))
            : mix(horizon, nadir,  smoothstep(0.0, -0.6, -h));
        vec3 sunDir = normalize(vec3(0.4, 0.6, -0.3));
        float sunDot = max(dot(rayDir, sunDir), 0.0);
        col += vec3(1.0, 0.95, 0.8) * pow(sunDot, 256.0) * 1.5;
        col += vec3(1.0, 0.8, 0.5) * pow(sunDot, 8.0) * 0.15;
        float cloudH = abs(h) + 0.1;
        vec2 cuv = rayDir.xz / cloudH * 1.5;
        float t = iTime * 0.015;
        float c1 = fbm(cuv + vec2(t, t * 0.3));
        float c2 = fbm(cuv * 2.0 + vec2(-t * 0.5, t * 0.7));
        float cloud = smoothstep(0.35, 0.65, c1 * 0.6 + c2 * 0.4);
        float cloudFade = smoothstep(0.0, 0.08, abs(h));
        col = mix(col, vec3(0.95, 0.95, 0.97), cloud * 0.85 * cloudFade);
    }
    else if (skyType == 2) {
        // --- Night Sky ---
        vec3 darkSky = vec3(0.02, 0.02, 0.06);
        vec3 horizonGlow = vec3(0.04, 0.03, 0.09);
        col = mix(horizonGlow, darkSky, smoothstep(-0.3, 0.5, h));
        vec2 starUV = vec2(sTheta, sPhi) * 80.0;
        vec2 starCell = floor(starUV);
        float starHash = hash21(starCell);
        if (starHash > 0.92) {
            vec2 starCenter = starCell + vec2(hash21(starCell + 1.0), hash21(starCell + 2.0));
            float d = length(starUV - starCenter);
            float twinkle = 0.7 + 0.3 * sin(iTime * (2.0 + starHash * 5.0) + starHash * 100.0);
            float brightness = smoothstep(0.09, 0.0, d) * twinkle;
            vec3 starCol = mix(vec3(0.8, 0.85, 1.0), vec3(1.0, 0.9, 0.7), starHash * 3.0 - 2.5);
            col += starCol * brightness * 0.8;
        }
        vec2 nebUV = vec2(sTheta, sPhi) * 1.5;
        float neb = fbm(nebUV + vec2(iTime * 0.005));
        col += vec3(0.1, 0.02, 0.15) * smoothstep(0.4, 0.7, neb) * 0.3;
        vec3 moonDir = normalize(vec3(-0.3, 0.55, 0.5));
        float moonDot = dot(rayDir, moonDir);
        col += vec3(0.8, 0.82, 0.9) * smoothstep(0.998, 0.9995, moonDot) * 0.6;
        col += vec3(0.3, 0.3, 0.4) * pow(max(moonDot, 0.0), 64.0) * 0.2;
    }
    else if (skyType == 3) {
        // --- Sunset ---
        vec3 sunDir = normalize(vec3(0.5, 0.08, -0.4));
        float sunDot = max(dot(rayDir, sunDir), 0.0);
        vec3 zenith   = vec3(0.12, 0.15, 0.40);
        vec3 mid      = vec3(0.55, 0.25, 0.45);
        vec3 horizon  = vec3(0.95, 0.55, 0.25);
        vec3 subHoriz = vec3(0.50, 0.25, 0.30);
        float absH = abs(h);
        col = h >= 0.0
            ? mix(horizon, mix(mid, zenith, smoothstep(0.15, 0.6, h)), smoothstep(0.0, 0.15, h))
            : mix(horizon, subHoriz, smoothstep(0.0, 0.4, -h));
        col += vec3(1.0, 0.7, 0.3) * pow(sunDot, 128.0) * 2.0;
        col += vec3(1.0, 0.5, 0.2) * pow(sunDot, 8.0) * 0.3;
        float cloudH = abs(h) + 0.15;
        vec2 cuv = rayDir.xz / cloudH * 2.0;
        float t = iTime * 0.01;
        float cl = fbm(cuv + vec2(t, t * 0.5));
        float cloud = smoothstep(0.4, 0.7, cl) * smoothstep(0.0, 0.1, abs(h));
        vec3 cloudCol = mix(vec3(0.95, 0.6, 0.3), vec3(0.8, 0.3, 0.4), smoothstep(0.0, 0.4, absH));
        col = mix(col, cloudCol, cloud * 0.7);
    }
    else if (skyType == 4) {
        // --- Overcast ---
        vec3 base = vec3(0.45, 0.47, 0.50);
        vec3 dark = vec3(0.30, 0.32, 0.35);
        vec3 light = vec3(0.58, 0.60, 0.62);
        float absH = abs(h);
        col = mix(light, base, smoothstep(0.0, 0.3, absH));
        vec2 cuv = vec2(sTheta, sPhi) * 3.0;
        float t = iTime * 0.008;
        float c1 = fbm(cuv + vec2(t, t * 0.4));
        float c2 = fbm(cuv * 2.5 + vec2(-t * 0.3, t));
        float cloudPattern = c1 * 0.6 + c2 * 0.4;
        col = mix(col, dark, smoothstep(0.35, 0.55, cloudPattern) * 0.5);
        col = mix(col, light, smoothstep(0.6, 0.8, cloudPattern) * 0.3);
        col += vec3(0.05) * (1.0 - smoothstep(0.0, 0.15, absH));
    }
    else if (skyType == 5) {
        // --- Aurora ---
        vec3 darkSky = vec3(0.01, 0.02, 0.05);
        vec3 horizGlow = vec3(0.03, 0.03, 0.08);
        col = mix(horizGlow, darkSky, smoothstep(-0.3, 0.5, h));
        vec2 starUV = vec2(sTheta, sPhi) * 80.0;
        vec2 starCell = floor(starUV);
        float starHash = hash21(starCell);
        if (starHash > 0.95) {
            vec2 starCenter = starCell + vec2(hash21(starCell + 1.0), hash21(starCell + 2.0));
            float d = length(starUV - starCenter);
            float twinkle = 0.8 + 0.2 * sin(iTime * 3.0 + starHash * 80.0);
            col += vec3(0.85, 0.9, 1.0) * smoothstep(0.08, 0.0, d) * twinkle * 0.6;
        }
        float t = iTime * 0.15;
        float band1 = sin(sTheta * 3.0 + t) * 0.5 + 0.5;
        float band2 = sin(sTheta * 5.0 - t * 1.3 + 1.0) * 0.5 + 0.5;
        float ripple = fbm(vec2(sTheta * 4.0 + t * 0.3, sPhi * 8.0 - t * 0.5));
        float aurora = (band1 * 0.6 + band2 * 0.4) * ripple;
        float elevMask = smoothstep(-0.1, 0.15, h) * smoothstep(0.7, 0.3, h);
        aurora *= elevMask;
        vec3 auroraCol = mix(vec3(0.1, 0.8, 0.3), vec3(0.3, 0.15, 0.7), band2);
        auroraCol = mix(auroraCol, vec3(0.1, 0.5, 0.8), ripple * 0.4);
        col += auroraCol * aurora * 0.6;
    }
)" R"(
    else if (skyType == 6) {
        // --- Contours ---
        float elevFreq = 30.0;
        float lonFreq = 20.0;
        float latLine = abs(fract(sPhi * elevFreq / 3.14159) - 0.5) * 2.0;
        float lonLine = abs(fract(sTheta * lonFreq / 6.28318) - 0.5) * 2.0;
        float meridian = 1.0 - smoothstep(0.02, 0.05, lonLine);
        vec3 bg = mix(vec3(0.02, 0.04, 0.03), vec3(0.05, 0.08, 0.06), smoothstep(-1.0, 1.0, h));
        float disp = fbm(vec2(sTheta * 3.0, sPhi * 3.0)) * 0.3;
        float noisyElev = abs(fract((sPhi + disp) * elevFreq / 3.14159) - 0.5) * 2.0;
        float noisyContour = 1.0 - smoothstep(0.02, 0.05, noisyElev);
        float indexLine = abs(fract((sPhi + disp) * elevFreq / 3.14159 * 0.2) - 0.5) * 2.0;
        float indexContour = 1.0 - smoothstep(0.01, 0.04, indexLine);
        col = bg;
        col = mix(col, vec3(0.15, 0.3, 0.4), meridian * 0.3);
        col = mix(col, vec3(0.2, 0.45, 0.3), noisyContour * 0.5);
        col = mix(col, vec3(0.35, 0.6, 0.4), indexContour * 0.7);
    }
    else if (skyType == 7) {
        // --- Voronoi ---
        float t = iTime * 0.06;
        vec2 uv = vec2(sTheta, sPhi) * 4.0;
        vec2 cell = floor(uv);
        float minDist = 10.0;
        float minDist2 = 10.0;
        vec2 nearestCell = cell;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                vec2 neighbor = cell + vec2(float(x), float(y));
                vec2 point = neighbor + vec2(hash21(neighbor + 0.1), hash21(neighbor + 7.3));
                point += 0.3 * vec2(sin(t + hash21(neighbor) * 6.28), cos(t * 0.7 + hash21(neighbor + 3.0) * 6.28));
                float d = length(uv - point);
                if (d < minDist) { minDist2 = minDist; minDist = d; nearestCell = neighbor; }
                else if (d < minDist2) { minDist2 = d; }
            }
        }
        float edgeDist = minDist2 - minDist;
        float edge = 1.0 - smoothstep(0.02, 0.06, edgeDist);
        float cellHash = hash21(nearestCell);
        vec3 bg = vec3(0.03, 0.03, 0.05);
        vec3 cellCol = hsv2rgb(vec3(cellHash * 0.3 + 0.55, 0.4, 0.08 + 0.04 * cellHash));
        vec3 edgeCol = mix(vec3(0.2, 0.4, 0.6), vec3(0.5, 0.3, 0.6), cellHash);
        col = mix(cellCol, bg, smoothstep(0.0, 0.8, minDist));
        col = mix(col, edgeCol, edge * 0.7);
    }
    else if (skyType == 8) {
        // --- Wireframe ---
        float latLine = abs(fract(sPhi * 18.0 / 3.14159) - 0.5) * 2.0;
        float lonLine = abs(fract(sTheta * 24.0 / 6.28318) - 0.5) * 2.0;
        float lat = 1.0 - smoothstep(0.02, 0.06, latLine);
        float lon = 1.0 - smoothstep(0.02, 0.06, lonLine);
        float wire = max(lat, lon);
        float equator = 1.0 - smoothstep(0.01, 0.04, abs(sPhi));
        float prime = 1.0 - smoothstep(0.01, 0.04, abs(sTheta));
        float prime180 = 1.0 - smoothstep(0.01, 0.04, abs(abs(sTheta) - 3.14159));
        col = vec3(0.02, 0.02, 0.03);
        col = mix(col, vec3(0.12, 0.22, 0.30), wire * 0.5);
        col = mix(col, vec3(0.25, 0.45, 0.55), max(equator, max(prime, prime180)) * 0.8);
        col += vec3(0.15, 0.3, 0.4) * lat * lon * 0.5;
        col *= h > 0.0 ? vec3(1.0, 1.0, 1.1) : vec3(1.1, 1.0, 1.0);
    }
    else if (skyType == 9) {
        // --- Noise ---
        float t = iTime * 0.05;
        vec2 uv = vec2(sTheta, sPhi) * 2.5;
        float n1 = fbm(uv + vec2(t, t * 0.7));
        float n2 = fbm(uv * 1.7 + vec2(-t * 0.6, t * 0.4) + 3.7);
        float warp = fbm(uv + vec2(n1, n2) * 1.5);
        col = vec3(0.03, 0.03, 0.05);
        col = mix(col, vec3(0.15, 0.1, 0.35), smoothstep(0.3, 0.5, warp));
        col = mix(col, vec3(0.05, 0.25, 0.3), smoothstep(0.5, 0.7, n1));
        col = mix(col, vec3(0.3, 0.12, 0.15), smoothstep(0.55, 0.75, n2) * 0.6);
        float ridge = abs(fract(warp * 6.0) - 0.5) * 2.0;
        float ridgeLine = 1.0 - smoothstep(0.03, 0.08, ridge);
        col += vec3(0.15, 0.2, 0.3) * ridgeLine * 0.4;
    }

    outColor = vec4(col, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Ground plane shader — large XZ quad at Y=0 with procedural surface
// Uniform groundType selects variant: 1=Grass, 2=Sand Dunes, 3=City
// ---------------------------------------------------------------------------

inline constexpr const char* kGroundVertShader = R"(
#version 150 core

in vec3 position;

uniform mat4  projection;
uniform mat4  view;
uniform float groundYOffset;
uniform float groundHills;

out vec2  vWorldXZ;
out float vDist;
out float vHeight;
out vec3  vNormal;

// Noise for vertex displacement
float ghash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float gnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = ghash(i);
    float b = ghash(i + vec2(1.0, 0.0));
    float c = ghash(i + vec2(0.0, 1.0));
    float d = ghash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float gfbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * gnoise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float terrainHeight(vec2 xz) {
    return gfbm(xz * 0.4) * 2.0 - 1.0;
}

void main()
{
    vec3 pos = position;
    pos.y += groundYOffset;
    vWorldXZ = pos.xz;

    // Hills displacement
    if (groundHills > 0.0) {
        float h = terrainHeight(pos.xz) * groundHills * 3.0;
        pos.y += h;
        vHeight = h;

        // Approximate normal via finite differences
        float eps = 0.1;
        float hx = terrainHeight(pos.xz + vec2(eps, 0.0)) * groundHills * 3.0;
        float hz = terrainHeight(pos.xz + vec2(0.0, eps)) * groundHills * 3.0;
        vNormal = normalize(vec3(h - hx, eps, h - hz));
    } else {
        vHeight = 0.0;
        vNormal = vec3(0.0, 1.0, 0.0);
    }

    vec4 viewPos = view * vec4(pos, 1.0);
    vDist = -viewPos.z;
    gl_Position = projection * viewPos;
}
)";

inline constexpr const char* kGroundFragShader = R"(
#version 150 core

in vec2  vWorldXZ;
in float vDist;
in float vHeight;
in vec3  vNormal;

uniform int   groundType;
uniform float iTime;
uniform vec3  fogColor;
uniform float groundHills;

out vec4 outColor;

// --- noise utilities ---
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise2D(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

void main()
{
    vec3 col = vec3(0.0);
    vec2 uv = vWorldXZ;

    if (groundType == 1) {
        // --- Grass ---
        float base = fbm(uv * 5.0);
        float detail = fbm(uv * 20.0 + vec2(3.7, 1.2));
        vec3 grassDark  = vec3(0.12, 0.30, 0.08);
        vec3 grassLight = vec3(0.22, 0.50, 0.15);
        vec3 grassDry   = vec3(0.35, 0.42, 0.12);
        col = mix(grassDark, grassLight, base);
        col = mix(col, grassDry, detail * 0.3);
        float wind = noise2D(uv * 8.0 + vec2(iTime * 0.3, iTime * 0.1));
        col *= 0.85 + 0.15 * wind;
    }
    else if (groundType == 2) {
        // --- Sand Dunes ---
        vec3 sandLight = vec3(0.82, 0.73, 0.55);
        vec3 sandDark  = vec3(0.65, 0.55, 0.38);
        vec3 sandWarm  = vec3(0.85, 0.68, 0.45);
        float ridges = fbm(uv * 1.5 + vec2(0.3, 0.7) * iTime * 0.01);
        float ripples = noise2D(uv * 15.0 + vec2(iTime * 0.05, 0.0));
        col = mix(sandDark, sandLight, ridges);
        col = mix(col, sandWarm, ripples * 0.2);
        float shadow = smoothstep(0.3, 0.6, ridges);
        col *= 0.7 + 0.3 * shadow;
    }
    else if (groundType == 3) {
        // --- City ---
        vec2 blockUV = uv * 3.0;
        vec2 cell = floor(blockUV);
        vec2 f    = fract(blockUV);
        float cellHash = hash21(cell);
        float streetW = 0.08;
        float isStreet = (f.x < streetW || f.x > 1.0 - streetW ||
                          f.y < streetW || f.y > 1.0 - streetW) ? 1.0 : 0.0;
        vec3 streetCol   = vec3(0.15, 0.15, 0.18);
        vec3 buildingCol = vec3(0.25, 0.25, 0.28) * (0.5 + 0.5 * cellHash);
        if (isStreet < 0.5) {
            float winHash = hash21(floor(blockUV * 4.0));
            float lit = step(0.6, winHash);
            vec3 winCol = mix(vec3(0.1, 0.1, 0.12), vec3(0.9, 0.8, 0.5), lit * 0.3);
            buildingCol = mix(buildingCol, winCol, 0.4);
        }
        col = mix(buildingCol, streetCol, isStreet);
        if (isStreet > 0.5) {
            float lightPos = fract(uv.x * 6.0 + uv.y * 6.0);
            float lightPulse = smoothstep(0.48, 0.5, lightPos) * smoothstep(0.52, 0.5, lightPos);
            col += vec3(1.0, 0.9, 0.6) * lightPulse * 0.3;
        }
    }
    else if (groundType == 4) {
        // --- Snow ---
        float base = fbm(uv * 4.0);
        float drift = fbm(uv * 12.0 + vec2(2.1, 0.7));
        vec3 snowWhite = vec3(0.90, 0.92, 0.95);
        vec3 snowBlue  = vec3(0.70, 0.75, 0.88);
        vec3 snowShadow = vec3(0.55, 0.60, 0.75);
        col = mix(snowWhite, snowBlue, base * 0.5);
        col = mix(col, snowShadow, smoothstep(0.4, 0.7, drift) * 0.35);
        float sparkle = noise2D(uv * 60.0);
        col += vec3(0.15) * smoothstep(0.92, 1.0, sparkle);
    }
    else if (groundType == 5) {
        // --- Ocean ---
        vec3 deepBlue = vec3(0.03, 0.08, 0.18);
        vec3 surfBlue = vec3(0.06, 0.18, 0.30);
        vec3 foam     = vec3(0.45, 0.55, 0.60);
        float t = iTime * 0.15;
        float w1 = noise2D(uv * 3.0 + vec2(t, t * 0.7));
        float w2 = noise2D(uv * 6.0 + vec2(-t * 0.5, t * 1.2));
        float w3 = fbm(uv * 1.5 + vec2(t * 0.3, -t * 0.2));
        float waves = w1 * 0.5 + w2 * 0.3 + w3 * 0.2;
        col = mix(deepBlue, surfBlue, waves);
        float foamLine = smoothstep(0.65, 0.75, waves);
        col = mix(col, foam, foamLine * 0.4);
        col += vec3(0.2, 0.25, 0.3) * pow(waves, 4.0) * 0.15;
    }
    else if (groundType == 6) {
        // --- Polar Grid ---
        float r = length(uv);
        float angle = atan(uv.y, uv.x);
        float ringLine = abs(fract(r * 3.0) - 0.5) * 2.0;
        float ring = 1.0 - smoothstep(0.03, 0.07, ringLine);
        float spokeLine = abs(fract(angle * 12.0 / 6.28318) - 0.5) * 2.0;
        float spoke = (1.0 - smoothstep(0.03, 0.07, spokeLine)) * smoothstep(0.1, 0.4, r);
        float cardLine = abs(fract(angle * 4.0 / 6.28318) - 0.5) * 2.0;
        float cardinal = (1.0 - smoothstep(0.02, 0.05, cardLine)) * smoothstep(0.1, 0.4, r);
        col = vec3(0.04, 0.04, 0.06);
        col = mix(col, vec3(0.15, 0.25, 0.35), max(ring, spoke) * 0.6);
        col = mix(col, vec3(0.3, 0.45, 0.55), cardinal * 0.7);
        col += vec3(0.4, 0.5, 0.6) * smoothstep(0.12, 0.0, r);
    }
    else if (groundType == 7) {
        // --- Contour Map --- (3x larger scale)
        float elev = fbm(uv * 0.27) * 2.0;
        float contourFreq = 12.0;
        float contourLine = abs(fract(elev * contourFreq) - 0.5) * 2.0;
        float contour = 1.0 - smoothstep(0.03, 0.07, contourLine);
        float indexLine = abs(fract(elev * contourFreq * 0.2) - 0.5) * 2.0;
        float indexContour = 1.0 - smoothstep(0.02, 0.05, indexLine);
        vec3 low  = vec3(0.04, 0.06, 0.04);
        vec3 mid  = vec3(0.06, 0.05, 0.04);
        vec3 high = vec3(0.05, 0.04, 0.05);
        vec3 bg = mix(low, mix(mid, high, smoothstep(1.0, 2.0, elev)), smoothstep(0.0, 1.0, elev));
        col = bg;
        col = mix(col, vec3(0.18, 0.35, 0.22), contour * 0.5);
        col = mix(col, vec3(0.30, 0.50, 0.32), indexContour * 0.8);
    }
    else if (groundType == 8) {
        // --- Voronoi --- (3x larger scale)
        vec2 vuv = uv * 0.83;
        vec2 cell = floor(vuv);
        float minDist = 10.0;
        float minDist2 = 10.0;
        vec2 nearestCell = cell;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                vec2 neighbor = cell + vec2(float(x), float(y));
                vec2 point = neighbor + vec2(hash21(neighbor + 0.1), hash21(neighbor + 7.3));
                float d = length(vuv - point);
                if (d < minDist) {
                    minDist2 = minDist;
                    minDist = d;
                    nearestCell = neighbor;
                } else if (d < minDist2) {
                    minDist2 = d;
                }
            }
        }
        float edgeDist = minDist2 - minDist;
        float edge = 1.0 - smoothstep(0.02, 0.05, edgeDist);
        float cellHash = hash21(nearestCell);
        vec3 bg = vec3(0.05, 0.05, 0.07) + vec3(0.03) * cellHash;
        vec3 edgeCol = mix(vec3(0.25, 0.4, 0.5), vec3(0.4, 0.25, 0.5), cellHash);
        col = bg;
        col = mix(col, edgeCol, edge * 0.7);
        col += vec3(0.3, 0.4, 0.5) * smoothstep(0.1, 0.0, minDist) * 0.3;
    }
    else if (groundType == 9) {
        // --- Terraces ---
        float elev = fbm(uv * 0.6) * 2.0;
        float steps = 10.0;
        float stepped = floor(elev * steps) / steps;
        float fr = fract(elev * steps);
        float cliff = 1.0 - smoothstep(0.02, 0.08, fr);
        float cliffTop = smoothstep(0.92, 0.98, fr);
        vec3 low  = vec3(0.06, 0.08, 0.10);
        vec3 mid  = vec3(0.10, 0.10, 0.13);
        vec3 high = vec3(0.14, 0.12, 0.15);
        vec3 plateauCol = mix(low, mix(mid, high, smoothstep(1.0, 2.0, stepped)), smoothstep(0.0, 1.0, stepped));
        col = plateauCol;
        col = mix(col, vec3(0.25, 0.3, 0.35), cliff * 0.6);
        col = mix(col, vec3(0.18, 0.22, 0.28), cliffTop * 0.4);
        col *= 0.9 + 0.1 * noise2D(uv * 30.0 + stepped * 7.0);
    }
    else if (groundType == 10) {
        // --- Cartesian Grid ---
        float gridFreq = 3.0;
        float lineX = abs(fract(uv.x * gridFreq) - 0.5) * 2.0;
        float lineY = abs(fract(uv.y * gridFreq) - 0.5) * 2.0;
        float grid = 1.0 - smoothstep(0.03, 0.07, min(lineX, lineY));
        // Major gridlines at 1/3 frequency
        float majorX = abs(fract(uv.x * gridFreq / 3.0) - 0.5) * 2.0;
        float majorY = abs(fract(uv.y * gridFreq / 3.0) - 0.5) * 2.0;
        float major = 1.0 - smoothstep(0.02, 0.05, min(majorX, majorY));
        // Axis lines (thicker, brighter)
        float axisX = 1.0 - smoothstep(0.01, 0.03, abs(uv.x));
        float axisY = 1.0 - smoothstep(0.01, 0.03, abs(uv.y));
        float axis = max(axisX, axisY);
        col = vec3(0.04, 0.04, 0.06);
        col = mix(col, vec3(0.12, 0.20, 0.30), grid * 0.5);
        col = mix(col, vec3(0.20, 0.35, 0.45), major * 0.7);
        col = mix(col, vec3(0.35, 0.55, 0.65), axis * 0.8);
    }

    // Hill shading — simple diffuse from a fixed light direction
    if (groundHills > 0.0) {
        vec3 lightDir = normalize(vec3(0.6, 1.0, 0.8));
        float diff = max(dot(vNormal, lightDir), 0.0);
        float hillShade = 0.5 + 0.5 * diff;
        col *= hillShade;
    }

    // Distance fog
    float fog = smoothstep(10.0, 120.0, vDist);
    col = mix(col, fogColor, fog);

    outColor = vec4(col, 1.0 - fog * 0.3);
}
)";

} // namespace xyzpan

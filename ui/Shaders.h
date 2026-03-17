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

out vec4 outColor;

void main()
{
    vec3 norm     = normalize(vNormal);
    float diff    = max(dot(norm, normalize(lightDir)), 0.0);
    float ambient = 0.35;
    float light   = ambient + (1.0 - ambient) * diff;
    outColor = vec4(nodeColor * light, opacity);
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

} // namespace xyzpan

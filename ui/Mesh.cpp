#include "Mesh.h"
#include <cmath>

namespace xyzpan {

// ---------------------------------------------------------------------------
// buildUnitSphere
// ---------------------------------------------------------------------------
SphereGeometry buildUnitSphere(int stacks, int slices)
{
    SphereGeometry geo;

    // Generate vertices: (stacks+1) rings × (slices+1) vertices per ring
    for (int s = 0; s <= stacks; ++s) {
        const float phi   = 3.14159265f * float(s) / float(stacks);
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (int sl = 0; sl <= slices; ++sl) {
            const float theta   = 2.0f * 3.14159265f * float(sl) / float(slices);
            const float sinT    = std::sin(theta);
            const float cosT    = std::cos(theta);

            const float x = sinPhi * cosT;
            const float y = cosPhi;
            const float z = sinPhi * sinT;

            // position
            geo.vertices.push_back(x);
            geo.vertices.push_back(y);
            geo.vertices.push_back(z);
            // normal (same as position for unit sphere)
            geo.vertices.push_back(x);
            geo.vertices.push_back(y);
            geo.vertices.push_back(z);
        }
    }

    // Generate triangle indices
    for (int s = 0; s < stacks; ++s) {
        for (int sl = 0; sl < slices; ++sl) {
            const unsigned a = static_cast<unsigned>(s       * (slices + 1) + sl);
            const unsigned b = static_cast<unsigned>((s + 1) * (slices + 1) + sl);
            const unsigned c = static_cast<unsigned>((s + 1) * (slices + 1) + sl + 1);
            const unsigned d = static_cast<unsigned>(s       * (slices + 1) + sl + 1);

            // Two triangles per quad
            geo.indices.push_back(a);
            geo.indices.push_back(b);
            geo.indices.push_back(c);

            geo.indices.push_back(a);
            geo.indices.push_back(c);
            geo.indices.push_back(d);
        }
    }

    return geo;
}

// ---------------------------------------------------------------------------
// buildCone — solid cone along +Y axis (base at Y=0, tip at Y=height)
// ---------------------------------------------------------------------------
SphereGeometry buildCone(float baseRadius, float height, int slices)
{
    SphereGeometry geo;

    // Tip vertex (index 0): position + normal pointing up
    geo.vertices.push_back(0.0f);
    geo.vertices.push_back(height);
    geo.vertices.push_back(0.0f);
    geo.vertices.push_back(0.0f);
    geo.vertices.push_back(1.0f);
    geo.vertices.push_back(0.0f);

    // Side slope for normals: rise = baseRadius, run = height
    const float slopeLen = std::sqrt(baseRadius * baseRadius + height * height);
    const float ny = baseRadius / slopeLen;  // outward component along Y
    const float nr = height / slopeLen;      // outward component radially

    // Base ring vertices (indices 1..slices)
    for (int i = 0; i < slices; ++i) {
        const float theta = 2.0f * 3.14159265f * float(i) / float(slices);
        const float cosT = std::cos(theta);
        const float sinT = std::sin(theta);

        // position on base circle
        geo.vertices.push_back(baseRadius * cosT);
        geo.vertices.push_back(0.0f);
        geo.vertices.push_back(baseRadius * sinT);
        // outward-facing normal for smooth shading
        geo.vertices.push_back(nr * cosT);
        geo.vertices.push_back(ny);
        geo.vertices.push_back(nr * sinT);
    }

    // Centre of base (index slices+1): for base cap
    const unsigned baseCenterIdx = static_cast<unsigned>(slices + 1);
    geo.vertices.push_back(0.0f);
    geo.vertices.push_back(0.0f);
    geo.vertices.push_back(0.0f);
    geo.vertices.push_back(0.0f);
    geo.vertices.push_back(-1.0f);
    geo.vertices.push_back(0.0f);

    // Side triangles: tip (0) → ring[i] → ring[i+1]
    for (int i = 0; i < slices; ++i) {
        const unsigned cur  = static_cast<unsigned>(1 + i);
        const unsigned next = static_cast<unsigned>(1 + (i + 1) % slices);
        geo.indices.push_back(0);
        geo.indices.push_back(cur);
        geo.indices.push_back(next);
    }

    // Base cap triangles: baseCenterIdx → ring[i+1] → ring[i] (reversed winding for downward normal)
    for (int i = 0; i < slices; ++i) {
        const unsigned cur  = static_cast<unsigned>(1 + i);
        const unsigned next = static_cast<unsigned>(1 + (i + 1) % slices);
        geo.indices.push_back(baseCenterIdx);
        geo.indices.push_back(next);
        geo.indices.push_back(cur);
    }

    return geo;
}

// ---------------------------------------------------------------------------
// buildRoomWireframe — 12 edges of a box
// ---------------------------------------------------------------------------
std::vector<float> buildRoomWireframe(float halfSize)
{
    const float h = halfSize;

    // 8 corners of the box
    // Convention: +X = right, +Y = up, +Z = toward viewer
    const float corners[8][3] = {
        {-h, -h, -h}, { h, -h, -h}, { h,  h, -h}, {-h,  h, -h},   // back face
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h},   // front face
    };

    // 12 edges: 4 bottom, 4 top, 4 vertical pillars
    const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},   // back face
        {4, 5}, {5, 6}, {6, 7}, {7, 4},   // front face
        {0, 4}, {1, 5}, {2, 6}, {3, 7},   // side connectors
    };

    std::vector<float> verts;
    verts.reserve(12 * 2 * 3);

    for (const auto& e : edges) {
        for (int v = 0; v < 2; ++v) {
            verts.push_back(corners[e[v]][0]);
            verts.push_back(corners[e[v]][1]);
            verts.push_back(corners[e[v]][2]);
        }
    }

    return verts;
}

// ---------------------------------------------------------------------------
// buildFloorGrid — N×N grid lines on the XZ plane (Y=0)
// ---------------------------------------------------------------------------
std::vector<float> buildFloorGrid(float halfSize, int divisions)
{
    std::vector<float> verts;
    const int lines = divisions + 1;
    verts.reserve(static_cast<size_t>(lines * 2 * 2 * 3));  // (N+1) lines per axis × 2 endpoints × 3 floats

    const float step = (2.0f * halfSize) / static_cast<float>(divisions);

    // Lines along Z axis (vary X, constant Z extent)
    for (int i = 0; i <= divisions; ++i) {
        const float x = -halfSize + step * static_cast<float>(i);
        // Start point
        verts.push_back(x);
        verts.push_back(0.0f);
        verts.push_back(-halfSize);
        // End point
        verts.push_back(x);
        verts.push_back(0.0f);
        verts.push_back(halfSize);
    }

    // Lines along X axis (vary Z, constant X extent)
    for (int i = 0; i <= divisions; ++i) {
        const float z = -halfSize + step * static_cast<float>(i);
        // Start point
        verts.push_back(-halfSize);
        verts.push_back(0.0f);
        verts.push_back(z);
        // End point
        verts.push_back(halfSize);
        verts.push_back(0.0f);
        verts.push_back(z);
    }

    return verts;
}

} // namespace xyzpan

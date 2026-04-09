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
// buildSphereWireframe — lat/long GL_LINES index pairs for unit sphere
// ---------------------------------------------------------------------------
std::vector<unsigned> buildSphereWireframe(int stacks, int slices)
{
    std::vector<unsigned> idx;
    // Same vertex layout as buildUnitSphere: vertex(s,sl) = s * (slices+1) + sl

    // Latitude lines: for each stack row, connect consecutive vertices around the ring
    for (int s = 0; s <= stacks; ++s) {
        for (int sl = 0; sl < slices; ++sl) {
            const unsigned a = static_cast<unsigned>(s * (slices + 1) + sl);
            const unsigned b = static_cast<unsigned>(s * (slices + 1) + sl + 1);
            idx.push_back(a);
            idx.push_back(b);
        }
    }

    // Longitude lines: for each slice, connect vertices down the meridian
    for (int sl = 0; sl < slices; ++sl) {
        for (int s = 0; s < stacks; ++s) {
            const unsigned a = static_cast<unsigned>(s       * (slices + 1) + sl);
            const unsigned b = static_cast<unsigned>((s + 1) * (slices + 1) + sl);
            idx.push_back(a);
            idx.push_back(b);
        }
    }

    return idx;
}

// ---------------------------------------------------------------------------
// buildSphereLatitudeRings — latitude rings only (no longitude meridians)
// ---------------------------------------------------------------------------
std::vector<unsigned> buildSphereLatitudeRings(int stacks, int slices)
{
    std::vector<unsigned> idx;
    for (int s = 0; s <= stacks; ++s) {
        for (int sl = 0; sl < slices; ++sl) {
            const unsigned a = static_cast<unsigned>(s * (slices + 1) + sl);
            const unsigned b = static_cast<unsigned>(s * (slices + 1) + sl + 1);
            idx.push_back(a);
            idx.push_back(b);
        }
    }
    return idx;
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
// buildConeWireframe — GL_LINES index pairs for cone wireframe
// ---------------------------------------------------------------------------
std::vector<unsigned> buildConeWireframe(int slices)
{
    std::vector<unsigned> idx;
    // Tip-to-base edges
    for (int i = 0; i < slices; ++i) {
        idx.push_back(0);  // tip
        idx.push_back(static_cast<unsigned>(1 + i));
    }
    // Base ring edges
    for (int i = 0; i < slices; ++i) {
        idx.push_back(static_cast<unsigned>(1 + i));
        idx.push_back(static_cast<unsigned>(1 + (i + 1) % slices));
    }
    return idx;
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

    // Colors per axis — varying shades of gold for visual differentiation
    const float xColor[3] = { 0xA6 / 255.0f, 0x8B / 255.0f, 0x3A / 255.0f };  // Gold Leaf Dark
    const float yColor[3] = { 0xD9 / 255.0f, 0xBE / 255.0f, 0x6E / 255.0f };  // Gold Leaf Light
    const float zColor[3] = { 0xC9 / 255.0f, 0xA8 / 255.0f, 0x4C / 255.0f };  // Gold Leaf (mid)

    // Edge definitions grouped by axis direction:
    //   X-axis (left/right): edges along ±X
    //   Y-axis (up/down):    edges along ±Y
    //   Z-axis (depth):      edges along ±Z
    struct ColoredEdge { int a, b; const float* color; };
    const ColoredEdge edges[] = {
        // X-axis edges (4)
        {0, 1, xColor}, {3, 2, xColor}, {4, 5, xColor}, {7, 6, xColor},
        // Y-axis edges (4)
        {0, 3, yColor}, {1, 2, yColor}, {4, 7, yColor}, {5, 6, yColor},
        // Z-axis edges (4)
        {0, 4, zColor}, {1, 5, zColor}, {2, 6, zColor}, {3, 7, zColor},
    };

    // Interleaved: [x, y, z, r, g, b] per vertex, 2 vertices per edge
    std::vector<float> verts;
    verts.reserve(12 * 2 * 6);

    for (const auto& e : edges) {
        for (int v = 0; v < 2; ++v) {
            const int idx = (v == 0) ? e.a : e.b;
            verts.push_back(corners[idx][0]);
            verts.push_back(corners[idx][1]);
            verts.push_back(corners[idx][2]);
            verts.push_back(e.color[0]);
            verts.push_back(e.color[1]);
            verts.push_back(e.color[2]);
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

// ---------------------------------------------------------------------------
// buildPyramid — 4-sided pyramid, base at Y=-0.5, apex at Y=+0.5
// ---------------------------------------------------------------------------
SphereGeometry buildPyramid()
{
    SphereGeometry geo;

    // Apex (0)
    const float apexY = 0.5f;
    // Base corners (1-4) at Y=-0.5
    const float baseY = -0.5f;
    const float b = 0.5f; // half-base

    struct V3 { float x, y, z; };
    const V3 apex  = {0.0f, apexY, 0.0f};
    const V3 base[4] = {
        {-b, baseY, -b},
        { b, baseY, -b},
        { b, baseY,  b},
        {-b, baseY,  b}
    };

    auto pushVert = [&](const V3& pos, const V3& norm) {
        geo.vertices.push_back(pos.x);
        geo.vertices.push_back(pos.y);
        geo.vertices.push_back(pos.z);
        geo.vertices.push_back(norm.x);
        geo.vertices.push_back(norm.y);
        geo.vertices.push_back(norm.z);
    };

    auto cross = [](const V3& a, const V3& b) -> V3 {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    };
    auto sub = [](const V3& a, const V3& b) -> V3 {
        return {a.x-b.x, a.y-b.y, a.z-b.z};
    };
    auto normalize = [](V3 v) -> V3 {
        float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        if (len > 0.0f) { v.x/=len; v.y/=len; v.z/=len; }
        return v;
    };

    // 4 side faces: apex + base[i] + base[(i+1)%4]
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        V3 e1 = sub(base[i], apex);
        V3 e2 = sub(base[j], apex);
        V3 n = normalize(cross(e1, e2));
        unsigned idx = static_cast<unsigned>(geo.vertices.size() / 6);
        pushVert(apex, n);
        pushVert(base[i], n);
        pushVert(base[j], n);
        geo.indices.push_back(idx);
        geo.indices.push_back(idx + 1);
        geo.indices.push_back(idx + 2);
    }

    // Bottom face (two triangles, normal pointing down)
    {
        V3 n = {0.0f, -1.0f, 0.0f};
        unsigned idx = static_cast<unsigned>(geo.vertices.size() / 6);
        for (int i = 0; i < 4; ++i) pushVert(base[i], n);
        geo.indices.push_back(idx);
        geo.indices.push_back(idx + 2);
        geo.indices.push_back(idx + 1);
        geo.indices.push_back(idx);
        geo.indices.push_back(idx + 3);
        geo.indices.push_back(idx + 2);
    }

    return geo;
}

// ---------------------------------------------------------------------------
// buildPyramidWireframe — GL_LINES for pyramid edges
// ---------------------------------------------------------------------------
std::vector<unsigned> buildPyramidWireframe()
{
    // Vertex layout: 0=apex, 1-4=base corners (use first 5 unique positions)
    // We'll use indices into the side-face vertices: face0 has apex at 0, base[0] at 1, base[1] at 2
    // But for wireframe it's simpler to define separate unique verts — however we share the same VBO.
    // Side face verts: face i has apex=i*3, base[i]=i*3+1, base[i+1]=i*3+2
    // We just need edges: apex→base[i] (4 edges) + base ring (4 edges)
    std::vector<unsigned> idx;
    // Apex to each base corner: apex vertex indices are 0, 3, 6, 9
    // base corner vertex indices: face0: base[0]=1, base[1]=2; face1: base[1]=4, base[2]=5; etc.
    // Apex edges (using each face's apex vertex):
    for (int i = 0; i < 4; ++i) {
        idx.push_back(static_cast<unsigned>(i * 3));      // apex of face i
        idx.push_back(static_cast<unsigned>(i * 3 + 1));   // base[i] of face i
    }
    // Base ring: base[i] → base[i+1]
    for (int i = 0; i < 4; ++i) {
        idx.push_back(static_cast<unsigned>(i * 3 + 1));       // base[i]
        idx.push_back(static_cast<unsigned>(i * 3 + 2));       // base[(i+1)%4]
    }
    return idx;
}

// ---------------------------------------------------------------------------
// buildCube — unit cube centred at origin
// ---------------------------------------------------------------------------
SphereGeometry buildCube()
{
    SphereGeometry geo;
    const float h = 0.5f;

    struct V3 { float x, y, z; };

    // 6 faces, each with 4 vertices and 2 triangles
    struct Face { V3 verts[4]; V3 normal; };
    const Face faces[6] = {
        // +X
        {{{ h,-h,-h},{ h, h,-h},{ h, h, h},{ h,-h, h}}, { 1, 0, 0}},
        // -X
        {{{-h,-h, h},{-h, h, h},{-h, h,-h},{-h,-h,-h}}, {-1, 0, 0}},
        // +Y
        {{{-h, h,-h},{ h, h,-h},{ h, h, h},{-h, h, h}}, { 0, 1, 0}},
        // -Y
        {{{-h,-h, h},{ h,-h, h},{ h,-h,-h},{-h,-h,-h}}, { 0,-1, 0}},
        // +Z
        {{{-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}}, { 0, 0, 1}},
        // -Z
        {{{ h,-h,-h},{-h,-h,-h},{-h, h,-h},{ h, h,-h}}, { 0, 0,-1}},
    };

    for (const auto& f : faces) {
        unsigned base = static_cast<unsigned>(geo.vertices.size() / 6);
        for (int i = 0; i < 4; ++i) {
            geo.vertices.push_back(f.verts[i].x);
            geo.vertices.push_back(f.verts[i].y);
            geo.vertices.push_back(f.verts[i].z);
            geo.vertices.push_back(f.normal.x);
            geo.vertices.push_back(f.normal.y);
            geo.vertices.push_back(f.normal.z);
        }
        geo.indices.push_back(base);
        geo.indices.push_back(base + 1);
        geo.indices.push_back(base + 2);
        geo.indices.push_back(base);
        geo.indices.push_back(base + 2);
        geo.indices.push_back(base + 3);
    }

    return geo;
}

// ---------------------------------------------------------------------------
// buildCubeWireframe — GL_LINES for cube edges
// ---------------------------------------------------------------------------
std::vector<unsigned> buildCubeWireframe()
{
    // Face 0 (+X): verts 0,1,2,3; Face 1 (-X): 4,5,6,7; etc.
    // 12 unique edges of a cube
    std::vector<unsigned> idx;
    // Each face has 4 edges but many are shared. Use face vertex indices:
    // +X face: 0-1, 1-2, 2-3, 3-0
    // -X face: 4-5, 5-6, 6-7, 7-4
    // Connect +X to -X via +Y, -Y, +Z, -Z faces
    // Simpler: just emit all 12 edges using face-local indices
    // +X: 0,1,2,3  -X: 4,5,6,7  +Y: 8,9,10,11  -Y: 12,13,14,15  +Z: 16,17,18,19  -Z: 20,21,22,23
    for (int face = 0; face < 6; ++face) {
        unsigned b = static_cast<unsigned>(face * 4);
        idx.push_back(b); idx.push_back(b+1);
        idx.push_back(b+1); idx.push_back(b+2);
        idx.push_back(b+2); idx.push_back(b+3);
        idx.push_back(b+3); idx.push_back(b);
    }
    return idx;
}

// ---------------------------------------------------------------------------
// buildOctahedron — unit octahedron (vertices at ±1 on each axis)
// ---------------------------------------------------------------------------
SphereGeometry buildOctahedron()
{
    SphereGeometry geo;

    struct V3 { float x, y, z; };
    // 6 vertices at ±1 on each axis
    const V3 verts[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    // 8 triangular faces
    const int faces[8][3] = {
        {0,2,4}, {0,4,3}, {0,3,5}, {0,5,2},
        {1,4,2}, {1,3,4}, {1,5,3}, {1,2,5}
    };

    auto normalize = [](V3 v) -> V3 {
        float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        if (len > 0.0f) { v.x/=len; v.y/=len; v.z/=len; }
        return v;
    };
    auto cross = [](const V3& a, const V3& b) -> V3 {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    };
    auto sub = [](const V3& a, const V3& b) -> V3 {
        return {a.x-b.x, a.y-b.y, a.z-b.z};
    };

    for (const auto& f : faces) {
        V3 a = verts[f[0]], b = verts[f[1]], c = verts[f[2]];
        V3 n = normalize(cross(sub(b,a), sub(c,a)));
        unsigned base = static_cast<unsigned>(geo.vertices.size() / 6);
        for (int i = 0; i < 3; ++i) {
            const V3& v = verts[f[i]];
            geo.vertices.push_back(v.x);
            geo.vertices.push_back(v.y);
            geo.vertices.push_back(v.z);
            geo.vertices.push_back(n.x);
            geo.vertices.push_back(n.y);
            geo.vertices.push_back(n.z);
        }
        geo.indices.push_back(base);
        geo.indices.push_back(base + 1);
        geo.indices.push_back(base + 2);
    }

    return geo;
}

// ---------------------------------------------------------------------------
// buildOctahedronWireframe — GL_LINES for octahedron edges
// ---------------------------------------------------------------------------
std::vector<unsigned> buildOctahedronWireframe()
{
    // 8 faces × 3 edges, but 12 unique edges. Each face has 3 verts at face*3..face*3+2.
    std::vector<unsigned> idx;
    for (int f = 0; f < 8; ++f) {
        unsigned b = static_cast<unsigned>(f * 3);
        idx.push_back(b); idx.push_back(b+1);
        idx.push_back(b+1); idx.push_back(b+2);
        idx.push_back(b+2); idx.push_back(b);
    }
    return idx;
}

// ---------------------------------------------------------------------------
// buildTorus — ring in XZ plane
// ---------------------------------------------------------------------------
SphereGeometry buildTorus(float majorRadius, float minorRadius, int majorSegs, int minorSegs)
{
    SphereGeometry geo;
    const float pi2 = 2.0f * 3.14159265f;

    // Vertex grid: (majorSegs+1) × (minorSegs+1)
    for (int i = 0; i <= majorSegs; ++i) {
        const float theta = pi2 * float(i) / float(majorSegs);
        const float cosT = std::cos(theta);
        const float sinT = std::sin(theta);

        for (int j = 0; j <= minorSegs; ++j) {
            const float phi = pi2 * float(j) / float(minorSegs);
            const float cosP = std::cos(phi);
            const float sinP = std::sin(phi);

            // Position on torus surface
            const float x = (majorRadius + minorRadius * cosP) * cosT;
            const float y = minorRadius * sinP;
            const float z = (majorRadius + minorRadius * cosP) * sinT;

            // Normal = direction from ring centre to surface point
            const float nx = cosP * cosT;
            const float ny = sinP;
            const float nz = cosP * sinT;

            geo.vertices.push_back(x);
            geo.vertices.push_back(y);
            geo.vertices.push_back(z);
            geo.vertices.push_back(nx);
            geo.vertices.push_back(ny);
            geo.vertices.push_back(nz);
        }
    }

    // Indices
    for (int i = 0; i < majorSegs; ++i) {
        for (int j = 0; j < minorSegs; ++j) {
            unsigned a = static_cast<unsigned>(i * (minorSegs + 1) + j);
            unsigned b = static_cast<unsigned>((i + 1) * (minorSegs + 1) + j);
            unsigned c = static_cast<unsigned>((i + 1) * (minorSegs + 1) + j + 1);
            unsigned d = static_cast<unsigned>(i * (minorSegs + 1) + j + 1);
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
// buildTorusWireframe — GL_LINES for torus rings
// ---------------------------------------------------------------------------
std::vector<unsigned> buildTorusWireframe(int majorSegs, int minorSegs)
{
    std::vector<unsigned> idx;
    // Major rings (circles around the tube)
    for (int i = 0; i <= majorSegs; ++i) {
        for (int j = 0; j < minorSegs; ++j) {
            unsigned a = static_cast<unsigned>(i * (minorSegs + 1) + j);
            unsigned b = static_cast<unsigned>(i * (minorSegs + 1) + j + 1);
            idx.push_back(a);
            idx.push_back(b);
        }
    }
    // Minor rings (circles along the major path)
    for (int j = 0; j <= minorSegs; ++j) {
        for (int i = 0; i < majorSegs; ++i) {
            unsigned a = static_cast<unsigned>(i * (minorSegs + 1) + j);
            unsigned b = static_cast<unsigned>((i + 1) * (minorSegs + 1) + j);
            idx.push_back(a);
            idx.push_back(b);
        }
    }
    return idx;
}

} // namespace xyzpan

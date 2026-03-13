#pragma once
#include <vector>

namespace xyzpan {

// ---------------------------------------------------------------------------
// Mesh geometry helpers for XYZPan GL view.
//
// All functions return interleaved vertex data.
// buildUnitSphere  → interleaved [x,y,z, nx,ny,nz] per vertex + separate indices
// buildRoomWireframe → interleaved [x,y,z] per vertex (GL_LINES pairs)
// buildFloorGrid   → interleaved [x,y,z] per vertex (GL_LINES pairs)
// ---------------------------------------------------------------------------

struct SphereGeometry {
    std::vector<float>    vertices; // interleaved: position(3) + normal(3) per vertex
    std::vector<unsigned> indices;  // triangle list indices
};

// Build a unit sphere (radius 1.0) centred at the origin.
// stacks = horizontal rings, slices = vertical segments.
// Typical: stacks=16, slices=16 for smooth appearance at small sizes.
SphereGeometry buildUnitSphere(int stacks, int slices);

// Build a box wireframe (12 edges of a cube) centred at origin.
// halfSize controls the half-extent of the box on each axis.
// Returns [x,y,z] flat vertex list — each pair of consecutive vertices is one edge.
std::vector<float> buildRoomWireframe(float halfSize);

// Build an NxN grid of lines on the XZ plane centred at origin.
// halfSize = half-width of the grid; divisions = number of cells per axis.
// Returns [x,y,z] flat vertex list (GL_LINES — each pair is one line segment).
std::vector<float> buildFloorGrid(float halfSize, int divisions);

} // namespace xyzpan

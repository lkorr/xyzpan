#pragma once
#include <juce_opengl/juce_opengl.h>

// GLM — included via juce_opengl transitive or local install.
// juce_opengl includes glm headers when JUCE_USE_GLM is set, but for safety
// we include glm directly (available via the glm::glm CMake target).
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace xyzpan {

// ---------------------------------------------------------------------------
// Camera — orbit camera with orthographic snap presets.
//
// In Orbit mode the camera orbits the world origin at a fixed distance.
// In a snap mode (TopDown/Side/Front) the camera snaps to an orthographic-
// looking perspective view aligned to one of the three cardinal planes.
// ---------------------------------------------------------------------------
struct Camera {
    // Orbit state
    float yaw    = 35.0f;   // degrees — horizontal rotation around Y axis
    float pitch  = 25.0f;   // degrees — vertical elevation angle
    float dist   = 3.5f;    // orbit radius from origin

    // Snap state
    bool  orthoSnap = false;

    enum class SnapView { Orbit, TopDown, Side, Front };
    SnapView activeSnap = SnapView::Orbit;

    // Returns a view matrix for the current camera state.
    glm::mat4 getViewMatrix() const;

    // Snap presets
    void setSnapTopDown();  // XY plane — looking straight down from above
    void setSnapSide();     // XZ plane — looking from the side (positive X axis)
    void setSnapFront();    // YZ plane — looking from the front (positive Y axis)
    void setOrbit();        // Return to free orbit mode with current yaw/pitch

    // Apply mouse drag to orbit camera (only active in Orbit mode).
    // dx/dy are screen-space pixel deltas.
    void applyMouseDrag(float dx, float dy);

private:
    // Saved orbit values when snapping (restore on setOrbit())
    float savedYaw_   = 35.0f;
    float savedPitch_ = 25.0f;
};

} // namespace xyzpan

#pragma once
#include <juce_opengl/juce_opengl.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// GLM — orbit camera math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "PositionBridge.h"

namespace xyzpan {

// ---------------------------------------------------------------------------
// XYZPanGLView — OpenGL 3.2 Core spatial visualization component.
//
// Renders:
//   - Dark alchemy background (#1a1108)
//   - Bronze room wireframe (box) scaled with R parameter
//   - Dark-earth floor grid scaled with R parameter
//   - Warm-gold listener node sphere at world origin
//   - Bright-gold source node sphere at PositionBridge position
//
// Input handling:
//   - Drag on empty space: orbits camera
//   - Drag on source node: moves source (posts X/Y/Z to APVTS via MessageManager)
//   - Three snap buttons drive setSnapView()
//
// Note: Takes juce::AudioProcessorValueTreeState& to avoid circular dependency
// between xyzpan_ui (compiled first) and plugin/ targets.
// ---------------------------------------------------------------------------
class XYZPanGLView : public juce::Component,
                     public juce::OpenGLRenderer
{
public:
    // apvts:  the processor's APVTS (for reading R and writing X/Y/Z)
    // proc:   the AudioProcessor base pointer (kept for future use / WeakReference)
    // bridge: the lock-free bridge written by processBlock
    XYZPanGLView(juce::AudioProcessorValueTreeState& apvts,
                 juce::AudioProcessor* proc,
                 xyzpan::PositionBridge& bridge);
    ~XYZPanGLView() override;

    // OpenGLRenderer overrides
    void newOpenGLContextCreated()  override;
    void renderOpenGL()             override;
    void openGLContextClosing()     override;

    // Mouse overrides (Component)
    void mouseDown(const juce::MouseEvent& e)  override;
    void mouseDrag(const juce::MouseEvent& e)  override;
    void mouseUp(const juce::MouseEvent& e)    override;
    void mouseMove(const juce::MouseEvent& e)  override;

    // Called by snap buttons in XYZPanEditor
    using SnapView = Camera::SnapView;
    void setSnapView(SnapView v);

private:
    // ------------------------------------------------------------------
    // GL helper: compile a shader program from vertex+fragment source
    // ------------------------------------------------------------------
    void compileShaders();

    // Upload mesh geometry to a VAO/VBO (line mesh: flat [x,y,z] list)
    void uploadLineVAO(GLuint& vao, GLuint& vbo, const std::vector<float>& vertices);

    // Upload sphere geometry (interleaved pos+normal) with index buffer
    void uploadSphereVAO();

    // Draw call helpers
    void drawLines(GLuint vao, int vertexCount,
                   const glm::vec3& color, float opacity,
                   const glm::mat4& modelMatrix);

    void drawSphere(const glm::vec3& position, float radius,
                    const glm::vec3& color, float opacity);

    // Project a 3D world position to screen (pixel) coordinates
    glm::vec2 projectToScreen(const glm::vec3& worldPos) const;

    // Hit-test: is the screen-space mouse position within radius of projected source?
    bool isNearSourceNode(const juce::Point<int>& mousePos) const;

    // Unproject screen delta to world-plane XYZ delta for dragging
    void computeDragDelta(const juce::Point<int>& screenDelta,
                          float& outDX, float& outDY, float& outDZ);

    // ------------------------------------------------------------------
    // GL context and shaders
    // ------------------------------------------------------------------
    juce::OpenGLContext glContext_;
    std::unique_ptr<juce::OpenGLShaderProgram> lineShader_;
    std::unique_ptr<juce::OpenGLShaderProgram> sphereShader_;

    // ------------------------------------------------------------------
    // GL resource handles (GLuint is a global typedef from GL headers)
    // ------------------------------------------------------------------
    GLuint vaoRoom_   = 0, vboRoom_   = 0;
    GLuint vaoGrid_   = 0, vboGrid_   = 0;
    GLuint vaoSphere_ = 0, vboSphere_ = 0, iboSphere_ = 0;

    int roomVertexCount_   = 0;
    int gridVertexCount_   = 0;
    int sphereIndexCount_  = 0;

    // Cached geometry
    std::vector<float>    sphereVerts_;
    std::vector<unsigned> sphereIdx_;

    // ------------------------------------------------------------------
    // Camera and projection
    // ------------------------------------------------------------------
    Camera      camera_;
    glm::mat4   projMatrix_  = glm::mat4(1.0f);
    glm::mat4   viewMatrix_  = glm::mat4(1.0f);

    // Cached projected source screen position for hit-testing
    glm::vec2   projectedSourcePos_ = {0.0f, 0.0f};

    // ------------------------------------------------------------------
    // Drag state
    // ------------------------------------------------------------------
    bool              isDraggingSource_ = false;
    bool              isSourceHovered_  = false;
    juce::Point<int>  lastDragPos_;

    // ------------------------------------------------------------------
    // References to processor APVTS, processor base, and bridge
    // ------------------------------------------------------------------
    juce::AudioProcessorValueTreeState& apvts_;
    juce::AudioProcessor*               proc_;   // kept for future WeakReference use
    xyzpan::PositionBridge&             bridge_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanGLView)
};

} // namespace xyzpan

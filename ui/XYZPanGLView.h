#pragma once
#include <juce_opengl/juce_opengl.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <array>

// GLM — orbit camera math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "PositionBridge.h"

namespace xyzpan {

// ---------------------------------------------------------------------------
// TrailBuffer — circular buffer of world-space positions for time-fading trails.
// Points fade to transparent based on age, not buffer position.
// ---------------------------------------------------------------------------
struct TrailBuffer {
    static constexpr int kCapacity = 48;
    static constexpr float kMinPushDist = 0.003f;
    static constexpr float kTrailLifetime = 0.8f;  // seconds before fully faded

    void push(const glm::vec3& pos, double timeSeconds) {
        if (count_ > 0) {
            const glm::vec3 diff = pos - positions_[head_];
            if (glm::dot(diff, diff) < kMinPushDist * kMinPushDist)
                return;
        }
        head_ = (head_ + 1) % kCapacity;
        positions_[head_] = pos;
        timestamps_[head_] = timeSeconds;
        if (count_ < kCapacity) ++count_;
    }

    void clear() { count_ = 0; head_ = 0; }

    // Write interleaved [x, y, z, alpha] newest-to-oldest into out.
    // Drops points older than kTrailLifetime. Returns number of points written.
    // Alpha combines time-based fade with positional fade so the tail always
    // graduates from opaque (head) to transparent (tail), even during fast motion.
    int fillVertexData(float* out, double nowSeconds) const {
        // First pass: count live points to compute positional gradient
        int liveCount = 0;
        for (int i = 0; i < count_; ++i) {
            const int idx = (head_ - i + kCapacity) % kCapacity;
            const float age = static_cast<float>(nowSeconds - timestamps_[idx]);
            if (age >= kTrailLifetime) break;
            ++liveCount;
        }
        if (liveCount == 0) return 0;

        // Second pass: write vertices with combined alpha
        int written = 0;
        for (int i = 0; i < liveCount; ++i) {
            const int idx = (head_ - i + kCapacity) % kCapacity;
            const float age = static_cast<float>(nowSeconds - timestamps_[idx]);
            const float timeFade = 1.0f - age / kTrailLifetime;
            // Positional fade: 1.0 at head (i=0), 0.0 at tail (i=liveCount-1)
            const float posFade = (liveCount > 1)
                ? 1.0f - static_cast<float>(i) / static_cast<float>(liveCount - 1)
                : 1.0f;
            const float alpha = timeFade * posFade;
            out[written * 4 + 0] = positions_[idx].x;
            out[written * 4 + 1] = positions_[idx].y;
            out[written * 4 + 2] = positions_[idx].z;
            out[written * 4 + 3] = alpha;
            ++written;
        }
        return written;
    }

private:
    std::array<glm::vec3, kCapacity> positions_{};
    std::array<double, kCapacity> timestamps_{};
    int head_  = 0;
    int count_ = 0;
};

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
    void mouseWheelMove(const juce::MouseEvent& e,
                        const juce::MouseWheelDetails& wheel) override;

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

    // Upload cone geometry (interleaved pos+normal) with index buffer
    void uploadConeVAO();

    // Draw call helpers
    void drawLines(GLuint vao, int vertexCount,
                   const glm::vec3& color, float opacity,
                   const glm::mat4& modelMatrix);

    void drawSphere(const glm::vec3& position, float radius,
                    const glm::vec3& color, float opacity);

    // Draw sphere VAO with arbitrary model matrix (for ellipsoids / custom transforms)
    void drawSphereWithModel(const glm::mat4& model, const glm::vec3& color, float opacity);

    // Draw the cone with an arbitrary model matrix (reuses sphere shader)
    void drawCone(const glm::mat4& model, const glm::vec3& color, float opacity);

    // Project a 3D world position to screen (pixel) coordinates
    glm::vec2 projectToScreen(const glm::vec3& worldPos) const;

    // Hit-test: is the screen-space mouse position within radius of projected source?
    bool isNearSourceNode(const juce::Point<int>& mousePos) const;

    // Unproject screen delta to world-plane XYZ delta for dragging
    void computeDragDelta(const juce::Point<int>& screenDelta,
                          float& outDX, float& outDY, float& outDZ);

    // Draw a fading trail from a TrailBuffer
    void drawTrail(TrailBuffer& trail, GLuint vao, GLuint vbo,
                   const glm::vec3& color, float baseOpacity, double nowSeconds);

    // ------------------------------------------------------------------
    // GL context and shaders
    // ------------------------------------------------------------------
    juce::OpenGLContext glContext_;
    std::unique_ptr<juce::OpenGLShaderProgram> lineShader_;
    std::unique_ptr<juce::OpenGLShaderProgram> sphereShader_;
    std::unique_ptr<juce::OpenGLShaderProgram> trailShader_;

    // ------------------------------------------------------------------
    // GL resource handles (GLuint is a global typedef from GL headers)
    // ------------------------------------------------------------------
    GLuint vaoRoom_   = 0, vboRoom_   = 0;
    GLuint vaoGrid_   = 0, vboGrid_   = 0;
    GLuint vaoSphere_ = 0, vboSphere_ = 0, iboSphere_ = 0;

    GLuint vaoCone_ = 0, vboCone_ = 0, iboCone_ = 0;

    GLuint vaoTrailSource_ = 0, vboTrailSource_ = 0;
    GLuint vaoTrailL_      = 0, vboTrailL_      = 0;
    GLuint vaoTrailR_      = 0, vboTrailR_      = 0;

    int roomVertexCount_   = 0;
    int gridVertexCount_   = 0;
    int sphereIndexCount_  = 0;
    int coneIndexCount_    = 0;

    // Cached geometry
    std::vector<float>    sphereVerts_;
    std::vector<unsigned> sphereIdx_;
    std::vector<float>    coneVerts_;
    std::vector<unsigned> coneIdx_;

    // Trail buffers and CPU staging
    TrailBuffer trailSource_, trailL_, trailR_;
    std::array<float, TrailBuffer::kCapacity * 4> trailVertexStaging_{};

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
    bool              isDraggingCamera_ = false;
    bool              isSourceHovered_  = false;
    juce::Point<int>  lastDragPos_;

    // ------------------------------------------------------------------
    // References to processor APVTS, processor base, and bridge
    // ------------------------------------------------------------------
    juce::AudioProcessorValueTreeState& apvts_;
    juce::AudioProcessor*               proc_;   // kept for future WeakReference use
    xyzpan::PositionBridge&             bridge_;

    // Frame rate throttle: 30fps when idle, 60fps when position moving or dragging
    double                              lastRenderTime_ = 0.0;
    xyzpan::SourcePositionSnapshot      lastSnap_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanGLView)
};

} // namespace xyzpan

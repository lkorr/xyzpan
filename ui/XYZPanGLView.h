#pragma once
#include <juce_opengl/juce_opengl.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <vector>
#include <array>
#include <functional>
#include <string>
#include <unordered_map>

// GLM — orbit camera math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.h"
#include "PositionBridge.h"
#include "ColorTheme.h"
#include "AvatarParams.h"

namespace xyzpan {

// ---------------------------------------------------------------------------
// TrailBuffer — circular buffer of world-space positions for time-fading trails.
// Points fade to transparent based on age, not buffer position.
// ---------------------------------------------------------------------------
struct TrailBuffer {
    static constexpr int kCapacity = 48;
    static constexpr float kMinPushDist = 0.003f;
    static constexpr float kTrailLifetime = 1.6f;  // seconds before fully faded

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
                     public juce::OpenGLRenderer,
                     public juce::AudioProcessorValueTreeState::Listener
{
public:
    // apvts:  the processor's APVTS (for reading R and writing X/Y/Z)
    // proc:   the AudioProcessor base pointer (kept for future use / WeakReference)
    // bridge: the lock-free bridge written by processBlock
    XYZPanGLView(juce::AudioProcessorValueTreeState& apvts,
                 juce::AudioProcessor* proc,
                 xyzpan::PositionBridge& bridge,
                 xyzpan::ForeignSourceBridge& foreignBridge,
                 std::shared_ptr<std::atomic<bool>> receivingBroadcast = nullptr);
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

    // Fired when a camera drag exits snap mode back to orbit
    std::function<void()> onSnapExited;

    // Runtime theme + avatar customization (thread-safe: called from message thread,
    // read on GL thread under SpinLock).
    void setColorTheme(const ColorTheme& theme);
    void setAvatarParams(const AvatarParams& params);

    // Set the index of the focused foreign source (-1 = none).
    // When set, the GL view draws a highlight ring around that source.
    void setFocusedForeignSource(int index) { focusedForeignIndex_.store(index, std::memory_order_relaxed); }

    // Set the name displayed above the own source node (message thread only)
    void setOwnInstanceName(const juce::String& name) { ownInstanceName_ = name; }

    // Instance list overlay — clickable list in top-left of GL view
    void setShowInstanceList(bool show) { showInstanceList_ = show; repaint(); }
    bool getShowInstanceList() const { return showInstanceList_; }

    // Callback when user clicks an instance in the overlay list.
    // Parameter: linked index (-1 = self, 0+ = foreign source index)
    std::function<void(int)> onInstanceClicked;

    // Callback when user double-clicks Self to rename.
    // Parameter: new name string
    std::function<void(const juce::String&)> onInstanceRenamed;

    // JUCE paint overlay — draws text labels above nodes on top of GL content
    void paint(juce::Graphics& g) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    // ------------------------------------------------------------------
    // GL helper: compile a shader program from vertex+fragment source
    // ------------------------------------------------------------------
    void compileShaders();

    // Upload mesh geometry to a VAO/VBO (line mesh: flat [x,y,z] list)
    void uploadLineVAO(GLuint& vao, GLuint& vbo, const std::vector<float>& vertices);

    // Upload interleaved [x,y,z, r,g,b] line VAO (for per-vertex colored lines)
    void uploadColorLineVAO(GLuint& vao, GLuint& vbo, const std::vector<float>& vertices);

    // Upload sphere geometry (interleaved pos+normal) with index buffer
    void uploadSphereVAO();

    // Upload cone geometry (interleaved pos+normal) with index buffer
    void uploadConeVAO();

    // Upload flat 2D arrow geometry (position-only triangles) for direction indicator
    void uploadArrow2DVAO();

    // Draw flat 2D arrow with the line shader (unlit flat color)
    void drawArrow2D(const glm::mat4& model, const glm::vec3& color, float opacity);

    // Draw call helpers
    void drawLines(GLuint vao, int vertexCount,
                   const glm::vec3& color, float opacity,
                   const glm::mat4& modelMatrix);

    // Draw per-vertex colored lines (room wireframe)
    void drawColorLines(GLuint vao, int vertexCount, float opacity,
                        const glm::mat4& modelMatrix);

    void drawSphere(const glm::vec3& position, float radius,
                    const glm::vec3& color, float opacity);

    // Draw sphere wireframe grid (lat/long GL_LINES) at given position with given radius
    void drawSphereWireframe(float radius, const glm::vec3& color, float opacity,
                             const glm::vec3& position = glm::vec3(0.0f));

    // Draw latitude-only sphere rings (no longitude meridians) for sound waves
    void drawSphereRings(float radius, const glm::vec3& color, float opacity,
                         const glm::vec3& position);

    // Draw expanding sound wave pulses radiating from a source node
    void drawSoundWaves(const glm::vec3& center, float sphereRadius,
                        float inputRms, const glm::vec3& color, double nowSeconds,
                        float intensity, float baseOpacity, float speed, int numWaves);

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

    struct CachedTextTexture { GLuint textureId = 0; float aspectRatio = 1.0f; };

    void uploadTextQuadVAO();
    CachedTextTexture getOrCreateTextTexture(const std::string& text);
    void drawBillboardLabel(const glm::vec3& worldPos, const std::string& text,
                            const glm::vec3& color, float opacity);

    // ------------------------------------------------------------------
    // GL context and shaders
    // ------------------------------------------------------------------
    juce::OpenGLContext glContext_;
    std::unique_ptr<juce::OpenGLShaderProgram> lineShader_;
    std::unique_ptr<juce::OpenGLShaderProgram> colorLineShader_;
    std::unique_ptr<juce::OpenGLShaderProgram> sphereShader_;
    std::unique_ptr<juce::OpenGLShaderProgram> trailShader_;
    std::unique_ptr<juce::OpenGLShaderProgram> textShader_;

    // ------------------------------------------------------------------
    // GL resource handles (GLuint is a global typedef from GL headers)
    // ------------------------------------------------------------------
    GLuint vaoRoom_   = 0, vboRoom_   = 0;
    GLuint vaoGrid_   = 0, vboGrid_   = 0;
    GLuint vaoSphere_ = 0, vboSphere_ = 0, iboSphere_ = 0;
    GLuint vaoSphereWire_ = 0, iboSphereWire_ = 0;
    int    sphereWireIndexCount_ = 0;
    GLuint vaoSphereRings_ = 0, iboSphereRings_ = 0;
    int    sphereRingsIndexCount_ = 0;

    GLuint vaoCone_ = 0, vboCone_ = 0, iboCone_ = 0;
    GLuint vaoArrow2D_ = 0, vboArrow2D_ = 0;
    int    arrow2DVertexCount_ = 0;

    GLuint vaoTrailSource_   = 0, vboTrailSource_   = 0;
    GLuint vaoTrailL_        = 0, vboTrailL_        = 0;
    GLuint vaoTrailR_        = 0, vboTrailR_        = 0;
    GLuint vaoTrailListener_ = 0, vboTrailListener_ = 0;

    GLuint vaoText_ = 0, vboText_ = 0;
    std::unordered_map<std::string, CachedTextTexture> textTextureCache_;

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
    TrailBuffer trailSource_, trailL_, trailR_, trailListener_;
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
    // Shared flag for callAsync lambdas — cleared in destructor so pending
    // async lambdas can detect that the GLView (and its APVTS ref) are gone.
    std::shared_ptr<std::atomic<bool>> glAlive_ = std::make_shared<std::atomic<bool>>(true);
    juce::AudioProcessor*               proc_;   // kept for future WeakReference use
    xyzpan::PositionBridge&             bridge_;
    xyzpan::ForeignSourceBridge&        foreignBridge_;

    // AudioProcessorValueTreeState::Listener override
    void parameterChanged(const juce::String& id, float newValue) override;

    // Runtime theme + avatar (written from message thread, read on GL thread)
    juce::SpinLock     customizeLock_;
    ColorTheme         glTheme_;
    AvatarParams       avatarParams_;
    std::atomic<int>   focusedForeignIndex_{-1};
    std::atomic<float>* waveIntensityParam_ = nullptr;
    std::atomic<float>* waveOpacityParam_   = nullptr;
    std::atomic<float>* waveSpeedParam_     = nullptr;
    std::atomic<float>* waveCountParam_     = nullptr;
    std::atomic<float>* showAudibleSphereParam_ = nullptr;
    std::atomic<float>* sourceSphereOpacityParam_ = nullptr;

    // Ear/eye type draw dispatchers
    // Nose type draw dispatchers
    void drawNoseCone(const glm::mat4& headRot, const AvatarParams& avatar,
                      const glm::vec3& noseCol, float hs, float alpha);
    void drawNoseButton(const glm::mat4& headRot, const AvatarParams& avatar,
                        const glm::vec3& noseCol, float hs, float alpha);
    void drawNoseSnout(const glm::mat4& headRot, const AvatarParams& avatar,
                       const glm::vec3& noseCol, float hs, float alpha);
    void drawNoseClown(const glm::mat4& headRot, const AvatarParams& avatar,
                       const glm::vec3& noseCol, float hs, float alpha);
    void drawNosePointed(const glm::mat4& headRot, const AvatarParams& avatar,
                         const glm::vec3& noseCol, float hs, float alpha);

    void drawEarsDefault(const glm::mat4& headRot, const AvatarParams& avatar,
                         const glm::vec3& earCol, float hs, float alpha);
    void drawEarsPointy(const glm::mat4& headRot, const AvatarParams& avatar,
                        const glm::vec3& earCol, float hs, float alpha);
    void drawEarsRound(const glm::mat4& headRot, const AvatarParams& avatar,
                       const glm::vec3& earCol, float hs, float alpha);
    void drawEarsCat(const glm::mat4& headRot, const AvatarParams& avatar,
                     const glm::vec3& earCol, float hs, float alpha);

    void drawHatParty(const glm::mat4& headRot, const AvatarParams& avatar,
                      const glm::vec3& hatCol, float hs, float alpha);
    void drawHatTopHat(const glm::mat4& headRot, const AvatarParams& avatar,
                       const glm::vec3& hatCol, float hs, float alpha);
    void drawHatHalo(const glm::mat4& headRot, const AvatarParams& avatar,
                     const glm::vec3& hatCol, float hs, float alpha);
    void drawHatBeanie(const glm::mat4& headRot, const AvatarParams& avatar,
                       const glm::vec3& hatCol, float hs, float alpha);
    void drawHatDevilHorns(const glm::mat4& headRot, const AvatarParams& avatar,
                           const glm::vec3& hatCol, float hs, float alpha);
    void drawHatPonytail(const glm::mat4& headRot, const AvatarParams& avatar,
                         const glm::vec3& hatCol, float hs, float alpha);

    void drawEyesNormal(const glm::mat4& headRot, const AvatarParams& avatar,
                        float hs, float headAlpha);
    void drawEyesGoogly(const glm::mat4& headRot, const AvatarParams& avatar,
                        float hs, float headAlpha);
    void drawEyesXEyes(const glm::mat4& headRot, const AvatarParams& avatar,
                       float hs, float headAlpha);
    void drawEyesCyclops(const glm::mat4& headRot, const AvatarParams& avatar,
                         float hs, float headAlpha);

    void updateGooglyPhysics(const SourcePositionSnapshot& snap, double now,
                             const glm::vec3& sourcePos,
                             const glm::vec3& lNodePos,
                             const glm::vec3& rNodePos,
                             bool stereoActive,
                             const AvatarParams& avatar);

    // Googly physics state (runtime-only, not persisted)
    struct GooglyEyeState {
        glm::vec2 pupilOffset{0.0f};
        glm::vec2 velocity{0.0f};
    };
    GooglyEyeState googlyLeft_, googlyRight_;
    float  prevListenerYaw_   = 0.0f;
    float  prevListenerPitch_ = 0.0f;
    double prevFrameTime_     = -1.0;

    // Head-follows-camera state
    bool  headFollowsActive_ = false;
    std::shared_ptr<std::atomic<bool>> drivingParamsFromCamera_ = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> receivingBroadcast_;  // shared with processor; suppresses cross-instance feedback
    float savedYawDeg_   = 0.0f;
    float savedPitchDeg_ = 0.0f;
    float savedRollDeg_  = 0.0f;

    // Instance name for paint() overlay labels (message thread only)
    juce::String   ownInstanceName_;

    // Instance list overlay state (message thread only)
    bool showInstanceList_ = true;  // visible by default for UI testing
    struct InstanceListEntry {
        juce::Rectangle<int> bounds;
        int linkedIndex;  // -1 = self, 0+ = foreign source index
    };
    std::vector<InstanceListEntry> instanceListHitBoxes_;

    // Smoothed input RMS for sound wave visualization (own + foreign sources)
    float smoothedRms_ = 0.0f;
    float foreignSmoothedRms_[kMaxLinkedSources] = {};

    // Smoothed foreign source positions for per-frame interpolation (GL thread only)
    struct SmoothedForeignPos {
        float x = 0.f, y = 0.f, z = 0.f;
        float lx = 0.f, ly = 0.f, lz = 0.f;
        float rx = 0.f, ry = 0.f, rz = 0.f;
        float sphereRadius = 1.732f;
    };
    SmoothedForeignPos foreignSmoothedPos_[kMaxLinkedSources];
    int foreignPrevCount_ = 0;

    // Inline rename editor for own instance name
    std::unique_ptr<juce::TextEditor> renameEditor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYZPanGLView)
};

} // namespace xyzpan

#include "XYZPanGLView.h"
#include "Shaders.h"
#include "Mesh.h"
#include "PositionBridge.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>

// ParamID string constants — duplicated here to avoid including plugin headers.
// These must stay in sync with plugin/ParamIDs.h.
namespace {
    constexpr const char* kParamR = "r";
    constexpr const char* kParamX = "x";
    constexpr const char* kParamY = "y";
    constexpr const char* kParamZ = "z";
}

using namespace juce::gl;

namespace xyzpan {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
XYZPanGLView::XYZPanGLView(juce::AudioProcessorValueTreeState& apvts,
                             juce::AudioProcessor* proc,
                             xyzpan::PositionBridge& bridge,
                             xyzpan::ForeignSourceBridge& foreignBridge,
                             std::shared_ptr<std::atomic<bool>> receivingBroadcast)
    : apvts_(apvts), proc_(proc), bridge_(bridge), foreignBridge_(foreignBridge),
      receivingBroadcast_(std::move(receivingBroadcast))
{
    // CRITICAL ORDER per RESEARCH.md: configure context BEFORE attachTo
    glContext_.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    glContext_.setRenderer(this);
    glContext_.setContinuousRepainting(true);
    glContext_.setComponentPaintingEnabled(true);  // composite JUCE children on top of GL
    glContext_.attachTo(*this);   // LAST

    // Listen for knob-driven listener param changes (bidirectional head-follows)
    apvts_.addParameterListener("listener_yaw",   this);
    apvts_.addParameterListener("listener_pitch",  this);
    apvts_.addParameterListener("listener_roll",   this);

    waveIntensityParam_    = apvts_.getRawParameterValue("wave_intensity");
    waveOpacityParam_      = apvts_.getRawParameterValue("wave_opacity");
    waveSpeedParam_        = apvts_.getRawParameterValue("wave_speed");
    waveCountParam_        = apvts_.getRawParameterValue("wave_count");
    showAudibleSphereParam_ = apvts_.getRawParameterValue("show_audible_sphere");
    sourceSphereOpacityParam_ = apvts_.getRawParameterValue("source_sphere_opacity");
}

XYZPanGLView::~XYZPanGLView()
{
    // Mark dead so any pending callAsync lambdas skip APVTS access
    glAlive_->store(false, std::memory_order_release);

    apvts_.removeParameterListener("listener_yaw",   this);
    apvts_.removeParameterListener("listener_pitch",  this);
    apvts_.removeParameterListener("listener_roll",   this);

    // CRITICAL: detach FIRST in destructor to safely call openGLContextClosing()
    glContext_.detach();
}

// ---------------------------------------------------------------------------
// newOpenGLContextCreated — compile shaders, build geometry, upload to GPU
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Helper: create a trail VAO/VBO with interleaved [vec3 pos, float alpha]
// ---------------------------------------------------------------------------
static void createTrailVAO(GLuint& vao, GLuint& vbo, int maxPoints)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Pre-allocate at max size; data uploaded each frame via glBufferSubData
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(maxPoints * 4 * sizeof(float)),
                 nullptr, GL_STREAM_DRAW);

    const GLsizei stride = 4 * sizeof(float);
    // attribute 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    // attribute 1: alpha (float)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void XYZPanGLView::newOpenGLContextCreated()
{
    compileShaders();

    // Build room wireframe (half-size=1.0 → fits in [-1,1]^3 coordinate space)
    // Interleaved [x,y,z, r,g,b] — per-vertex colored by axis
    std::vector<float> roomVerts = buildRoomWireframe(1.0f);
    roomVertexCount_ = static_cast<int>(roomVerts.size()) / 6;
    uploadColorLineVAO(vaoRoom_, vboRoom_, roomVerts);

    // Build floor grid (8×8 divisions)
    std::vector<float> gridVerts = buildFloorGrid(1.0f, 8);
    gridVertexCount_ = static_cast<int>(gridVerts.size()) / 3;
    uploadLineVAO(vaoGrid_, vboGrid_, gridVerts);

    // Build sphere geometry and upload
    auto sphere = buildUnitSphere(16, 16);
    sphereVerts_ = std::move(sphere.vertices);
    sphereIdx_   = std::move(sphere.indices);
    sphereIndexCount_ = static_cast<int>(sphereIdx_.size());
    uploadSphereVAO();

    // Build cone geometry for forward arrow and upload
    auto cone = buildCone(1.0f, 1.0f, 16);
    coneVerts_ = std::move(cone.vertices);
    coneIdx_   = std::move(cone.indices);
    coneIndexCount_ = static_cast<int>(coneIdx_.size());
    uploadConeVAO();

    // Build flat 2D arrow for direction indicator
    uploadArrow2DVAO();

    // Create trail VAO/VBOs
    createTrailVAO(vaoTrailSource_, vboTrailSource_, TrailBuffer::kCapacity);
    createTrailVAO(vaoTrailL_,      vboTrailL_,      TrailBuffer::kCapacity);
    createTrailVAO(vaoTrailR_,      vboTrailR_,      TrailBuffer::kCapacity);
    createTrailVAO(vaoTrailListener_, vboTrailListener_, TrailBuffer::kCapacity);

    // Text billboard quad
    uploadTextQuadVAO();

}

// ---------------------------------------------------------------------------
// parameterChanged — bidirectional head-follows: knob → camera
// ---------------------------------------------------------------------------
void XYZPanGLView::parameterChanged(const juce::String& id, float newValue)
{
    if (!headFollowsActive_ || isDraggingCamera_
        || drivingParamsFromCamera_->load(std::memory_order_relaxed)
        || (receivingBroadcast_ && receivingBroadcast_->load(std::memory_order_relaxed)))
        return;

    if (id == "listener_yaw")
        camera_.yaw = newValue;              // both use -180..180°
    else if (id == "listener_pitch")
        camera_.pitch = newValue;
    else if (id == "listener_roll")
        camera_.roll = newValue;
    camera_.syncQuatFromEuler();
}

// ---------------------------------------------------------------------------
// setColorTheme / setAvatarParams — called from message thread
// ---------------------------------------------------------------------------
void XYZPanGLView::setColorTheme(const ColorTheme& theme)
{
    const juce::SpinLock::ScopedLockType lock(customizeLock_);
    glTheme_ = theme;
}

void XYZPanGLView::setAvatarParams(const AvatarParams& params)
{
    const juce::SpinLock::ScopedLockType lock(customizeLock_);
    avatarParams_ = params;
}

// ---------------------------------------------------------------------------
// renderOpenGL — called every frame on the GL thread
// ---------------------------------------------------------------------------
void XYZPanGLView::renderOpenGL()
{
    jassert(juce::OpenGLHelpers::isContextActive());

    // Snapshot theme + avatar under lock (fast — POD copies)
    ColorTheme   theme;
    AvatarParams avatar;
    {
        const juce::SpinLock::ScopedLockType lock(customizeLock_);
        theme  = glTheme_;
        avatar = avatarParams_;
    }

    // Read current position snapshot from the lock-free bridge
    const auto snap = bridge_.read();

    // Viewport
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0) return;
    glViewport(0, 0, w, h);

    // Clear with theme background color
    juce::OpenGLHelpers::clear(juce::Colour(theme.background));
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Walker listener position in GL coordinates (computed early for camera)
    const glm::vec3 listenerPos(snap.listenerPosX, snap.listenerPosZ, -snap.listenerPosY);
    const bool walkerActive = (std::abs(snap.listenerPosX) > 1e-7f ||
                               std::abs(snap.listenerPosY) > 1e-7f ||
                               std::abs(snap.listenerPosZ) > 1e-7f);

    // Update matrices — camera follows walker listener position
    viewMatrix_ = camera_.getViewMatrix(listenerPos);
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const float nearPlane = std::max(0.001f, camera_.dist * 0.1f);
    projMatrix_ = glm::perspective(glm::radians(45.0f), aspect, nearPlane, 100.0f);

    // Scale room wireframe + floor grid by R so the cube boundary matches
    // the effective coordinate range (params * R).
    const float r = [&]() -> float {
        if (auto* a = apvts_.getRawParameterValue(kParamR))
            return a->load();
        return 1.0f;
    }();
    const glm::mat4 roomModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(r, r, r));

    // Coordinate convention mapping:
    //   XYZPan X = left/right  → GL X
    //   XYZPan Y = front/back  → GL -Z (GL +Z is toward viewer, +Y is front in XYZPan)
    //   XYZPan Z = up/down     → GL Y
    const glm::vec3 sourcePos(snap.x, snap.z, -snap.y);

    // Sphere radius from bridge — quartered for visual scaling so the rendered
    // boundary better matches perceived distance cues (DSP uses full value).
    const float sr = snap.sphereRadius * 0.25f;

    // Smooth input RMS for sound wave visualization (~60Hz GL thread)
    constexpr float kRmsAttack = 0.3f;   // fast attack
    constexpr float kRmsRelease = 0.05f; // slow release
    const float rmsCoeff = (snap.inputRms > smoothedRms_) ? kRmsAttack : kRmsRelease;
    smoothedRms_ += rmsCoeff * (snap.inputRms - smoothedRms_);

    // Wave visualization params from dev panel
    const float waveIntensity = waveIntensityParam_ ? waveIntensityParam_->load(std::memory_order_relaxed) : 3.5f;
    const float waveBaseOpacity = waveOpacityParam_ ? waveOpacityParam_->load(std::memory_order_relaxed) : 0.02f;
    const float waveSpeed = waveSpeedParam_ ? waveSpeedParam_->load(std::memory_order_relaxed) : 0.3f;
    const int   waveCount = waveCountParam_ ? static_cast<int>(waveCountParam_->load(std::memory_order_relaxed)) : 3;

    // Compute source opacity: full at close range, ~10% at sphere boundary
    const float distFrac = std::clamp(snap.distance / sr, 0.0f, 1.0f);
    const float sourceOpacity = 0.1f + 0.9f * (1.0f - distFrac);

    // Stereo L/R node positions (same coordinate mapping)
    const bool stereoActive = snap.stereoWidth > 0.0f;
    const glm::vec3 lNodePos(snap.lNodeX, snap.lNodeZ, -snap.lNodeY);
    const glm::vec3 rNodePos(snap.rNodeX, snap.rNodeZ, -snap.rNodeY);

    // Current time for trail timestamps
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    // Push to trail buffers — skip center trail when stereo orbit is active
    if (!stereoActive)
        trailSource_.push(sourcePos, now);
    else
        trailSource_.clear();

    if (stereoActive) {
        trailL_.push(lNodePos, now);
        trailR_.push(rNodePos, now);
    } else {
        trailL_.clear();
        trailR_.clear();
    }

    // Listener trail — only when walker is active
    if (walkerActive)
        trailListener_.push(listenerPos, now);
    else
        trailListener_.clear();

    // Draw room wireframe — scaled by R, per-vertex axis colors
    drawColorLines(vaoRoom_, roomVertexCount_, 0.7f, roomModelMatrix);

    // Draw floor grid — scaled by R (model = roomModelMatrix)
    {
        drawLines(vaoGrid_, gridVertexCount_, theme.glGrid, 0.3f, roomModelMatrix);
    }

    // Begin sphere/cone shader batch -- bind once, upload shared uniforms once
    if (sphereShader_ && vaoSphere_ != 0 && sphereIndexCount_ > 0) {
        sphereShader_->use();
        sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
        sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
        const glm::vec3 lightDir = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
        sphereShader_->setUniform("lightDir", lightDir.x, lightDir.y, lightDir.z);
        glBindVertexArray(vaoSphere_);
    }

    // Fade listener head to transparent as camera zooms in close.
    // Fully opaque at dist >= 0.5, fully transparent at dist <= 0.15.
    const float headAlpha = std::clamp((camera_.dist - 0.05f) / (0.5f - 0.05f), 0.0f, 1.0f);

    // Head-follows-camera: when toggle is ON, map camera orbit angles
    // to listener yaw/pitch params regardless of zoom level.
    {
        const bool toggleOn = apvts_.getRawParameterValue("head_follows_camera")->load() >= 0.5f;

        if (toggleOn && !headFollowsActive_) {
            // Entering head-follows mode: save current param values
            savedYawDeg_   = apvts_.getRawParameterValue("listener_yaw")->load();
            savedPitchDeg_ = apvts_.getRawParameterValue("listener_pitch")->load();
            savedRollDeg_  = apvts_.getRawParameterValue("listener_roll")->load();
            headFollowsActive_ = true;
        }

        if (headFollowsActive_ && toggleOn && !isDraggingCamera_) {
            // Camera follows head: read listener angles and apply to camera.
            // Skipped during drag — the quaternion is authoritative while dragging.
            camera_.yaw   = glm::degrees(snap.listenerYaw);
            camera_.pitch = glm::degrees(snap.listenerPitch);
            camera_.roll  = glm::degrees(snap.listenerRoll);
            camera_.syncQuatFromEuler();
        }

        if (headFollowsActive_ && !toggleOn) {
            // Exiting head-follows mode: retain current camera-driven angles
            headFollowsActive_ = false;
        }
    }

    // Draw listener node at origin — themed color, avatar-parameterized geometry
    if (headAlpha > 0.0f) {
        // Write depth only when fully opaque to avoid visual artifacts
        if (headAlpha < 1.0f)
            glDepthMask(GL_FALSE);

        const float hs = avatar.headSize;

        // Resolve effective colors: avatar override (non-zero) takes priority over theme.
        // headColor applies to head and ears; noseColor applies to nose.
        const glm::vec3 headCol  = avatar.headColor  ? toVec3(avatar.headColor)  : theme.glListenerHead;
        const glm::vec3 earCol   = avatar.headColor  ? toVec3(avatar.headColor)  : theme.glEar;
        const glm::vec3 noseCol  = avatar.noseColor  ? toVec3(avatar.noseColor)  : theme.glNose;

        // Listener head transform: translate to walker position, then rotate
        glm::mat4 headRot = glm::translate(glm::mat4(1.0f), listenerPos);
        headRot = glm::rotate(headRot, snap.listenerYaw,  glm::vec3(0.0f, 1.0f, 0.0f));
        headRot = glm::rotate(headRot, snap.listenerPitch, glm::vec3(1.0f, 0.0f, 0.0f));
        headRot = glm::rotate(headRot, snap.listenerRoll,  glm::vec3(0.0f, 0.0f, -1.0f));

        // Head sphere — rotated by headRot so elongation follows face orientation,
        // Y-scaled by headElongation, uniformly scaled by headSize
        {
            constexpr float kBaseHeadRadius = 0.045f;
            glm::mat4 headModel = headRot;
            headModel = glm::scale(headModel, glm::vec3(kBaseHeadRadius * hs,
                                                         kBaseHeadRadius * hs * avatar.headElongation,
                                                         kBaseHeadRadius * hs));
            drawSphereWithModel(headModel, headCol, headAlpha);
        }

        // Nose — dispatched by nose type
        if (avatar.noseType != kNoseNone) {
            switch (static_cast<NoseType>(avatar.noseType)) {
                case kNoseButton:  drawNoseButton(headRot, avatar, noseCol, hs, headAlpha);  break;
                case kNoseSnout:   drawNoseSnout(headRot, avatar, noseCol, hs, headAlpha);   break;
                case kNoseClown:   drawNoseClown(headRot, avatar, noseCol, hs, headAlpha);   break;
                case kNosePointed: drawNosePointed(headRot, avatar, noseCol, hs, headAlpha); break;
                default:           drawNoseCone(headRot, avatar, noseCol, hs, headAlpha);    break;
            }
        }

        // Ears — dispatched by ear type
        switch (static_cast<EarType>(avatar.earType)) {
            case kEarPointy:  drawEarsPointy(headRot, avatar, earCol, hs, headAlpha); break;
            case kEarRound:   drawEarsRound(headRot, avatar, earCol, hs, headAlpha);  break;
            case kEarCat:     drawEarsCat(headRot, avatar, earCol, hs, headAlpha);    break;
            default:          drawEarsDefault(headRot, avatar, earCol, hs, headAlpha); break;
        }

        // Eyes — dispatched by eye type
        if (avatar.eyeType != kEyeNone) {
            if (avatar.eyeType != kEyeGoogly) {
                googlyLeft_ = {};
                googlyRight_ = {};
            }
            switch (static_cast<EyeType>(avatar.eyeType)) {
                case kEyeGoogly:  updateGooglyPhysics(snap, now, sourcePos, lNodePos, rNodePos, stereoActive, avatar);
                                  drawEyesGoogly(headRot, avatar, hs, headAlpha);  break;
                case kEyeXEyes:   drawEyesXEyes(headRot, avatar, hs, headAlpha);   break;
                case kEyeCyclops: drawEyesCyclops(headRot, avatar, hs, headAlpha); break;
                case kEyeNone:    break;
                default:          drawEyesNormal(headRot, avatar, hs, headAlpha);  break;
            }
        } else {
            googlyLeft_ = {};
            googlyRight_ = {};
        }

        // Hats — dispatched by hat type
        if (avatar.hatType != kHatNone) {
            const glm::vec3 hatCol = avatar.hatColor ? toVec3(avatar.hatColor) : theme.glHat;
            switch (static_cast<HatType>(avatar.hatType)) {
                case kHatParty:  drawHatParty(headRot, avatar, hatCol, hs, headAlpha);  break;
                case kHatTopHat: drawHatTopHat(headRot, avatar, hatCol, hs, headAlpha); break;
                case kHatHalo:   drawHatHalo(headRot, avatar, hatCol, hs, headAlpha);   break;
                case kHatBeanie:     drawHatBeanie(headRot, avatar, hatCol, hs, headAlpha);     break;
                case kHatDevilHorns: drawHatDevilHorns(headRot, avatar, hatCol, hs, headAlpha); break;
                case kHatPonytail:   drawHatPonytail(headRot, avatar, hatCol, hs, headAlpha);   break;
                default: break;
            }
        }

        // Direction arrow — flat 2D arrow pointing forward from the face
        {
            constexpr float kGap   = 0.065f;  // gap from head center
            constexpr float kLen   = 0.08f;   // total arrow length
            constexpr float kWidth = 0.02f;   // half-width scale

            // Arrow mesh points along +Y. Rotate so +Y maps to -Z (forward).
            glm::mat4 ma = headRot;
            ma = glm::translate(ma, glm::vec3(0.0f, 0.0f, -kGap * hs));
            ma = glm::rotate(ma, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            ma = glm::scale(ma, glm::vec3(kWidth * hs, kLen * hs, 1.0f));
            drawArrow2D(ma, noseCol, 0.9f * headAlpha);
        }

        if (headAlpha < 1.0f)
            glDepthMask(GL_TRUE);
    }

    // Draw audible radius sphere + wireframe centered on the source node
    {
        const bool showSphere = showAudibleSphereParam_ == nullptr
                                || showAudibleSphereParam_->load(std::memory_order_relaxed) >= 0.5f;
        glDepthMask(GL_FALSE);
        if (showSphere) {
            drawSphere(sourcePos, sr, theme.glAudibleSphere, 0.04f);
            drawSphereWireframe(sr, theme.glAudibleSphere, 0.025f, sourcePos);
        }

        // Re-establish sphere shader for sound wave filled spheres
        sphereShader_->use();
        sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
        sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
        const glm::vec3 lightDir = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
        sphereShader_->setUniform("lightDir", lightDir.x, lightDir.y, lightDir.z);
        glBindVertexArray(vaoSphere_);

        // Sound wave pulses — filled transparent spheres expanding outward
        drawSoundWaves(sourcePos, sr, smoothedRms_, theme.glAudibleSphere, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);

        glDepthMask(GL_TRUE);
    }

    // Draw source node at current position
    // 0.8x base size; 10% opacity when stereo split is active.
    // When stereo is active, skip depth write so the ghost center doesn't
    // occlude the L/R nodes orbiting behind it.
    {
        const float sphereOpacityMul = sourceSphereOpacityParam_
            ? sourceSphereOpacityParam_->load(std::memory_order_relaxed) : 1.0f;
        const glm::vec3 sourceColor = isSourceHovered_
            ? theme.glSourceHover
            : theme.glSourceNormal;
        const float mainOpacity = (stereoActive ? 0.1f : sourceOpacity) * sphereOpacityMul;
        const float mainRadius = stereoActive ? 0.0375f : 0.048f;
        if (mainOpacity > 0.0f) {
            if (stereoActive) glDepthMask(GL_FALSE);
            drawSphere(sourcePos, mainRadius, sourceColor, mainOpacity);
            if (stereoActive) glDepthMask(GL_TRUE);
        }
    }

    // Draw L/R stereo node spheres (smaller than center, only when stereo active)
    // Each node's opacity is based on its own distance to the listener (origin).
    // Draw back-to-front (farther node first) so the nearer semi-transparent
    // node blends correctly on top of the farther one.
    if (stereoActive) {
        const float lDistFrac = std::clamp(glm::length(lNodePos) / sr, 0.0f, 1.0f);
        const float rDistFrac = std::clamp(glm::length(rNodePos) / sr, 0.0f, 1.0f);
        const float lOpacity = 0.1f + 0.9f * (1.0f - lDistFrac);
        const float rOpacity = 0.1f + 0.9f * (1.0f - rDistFrac);

        const glm::vec3 leftColor  = theme.glStereoL;
        const glm::vec3 rightColor = theme.glStereoR;

        // Camera-space Z for depth sorting (more negative = farther from camera)
        const float lCamZ = (viewMatrix_ * glm::vec4(lNodePos, 1.0f)).z;
        const float rCamZ = (viewMatrix_ * glm::vec4(rNodePos, 1.0f)).z;

        if (lCamZ < rCamZ) {
            // L is farther — draw L first, then R on top
            drawSphere(lNodePos, 0.045f, leftColor, lOpacity);
            drawSphere(rNodePos, 0.045f, rightColor, rOpacity);
        } else {
            // R is farther — draw R first, then L on top
            drawSphere(rNodePos, 0.045f, rightColor, rOpacity);
            drawSphere(lNodePos, 0.045f, leftColor, lOpacity);
        }
    }

    // Foreign (linked instance) source nodes + audible radius spheres
    const auto fp = foreignBridge_.read();
    // Snap smoothed positions for newly appeared sources (avoid fly-in from origin)
    if (fp.count > foreignPrevCount_) {
        for (int i = foreignPrevCount_; i < fp.count; ++i) {
            const int ri = std::clamp(i, 0, static_cast<int>(kMaxLinkedSources) - 1);
            const auto& fs = fp.sources[i];
            foreignSmoothedPos_[ri] = { fs.x, fs.y, fs.z,
                                        fs.lNodeX, fs.lNodeY, fs.lNodeZ,
                                        fs.rNodeX, fs.rNodeY, fs.rNodeZ,
                                        fs.sphereRadius };
        }
    }
    foreignPrevCount_ = fp.count;

    {
        static constexpr glm::vec3 kPalette[8] = {
            {0.60f, 0.40f, 0.80f},  // purple
            {0.30f, 0.70f, 0.50f},  // teal-green
            {0.80f, 0.45f, 0.30f},  // burnt orange
            {0.40f, 0.55f, 0.85f},  // steel blue
            {0.75f, 0.35f, 0.55f},  // rose
            {0.50f, 0.70f, 0.35f},  // olive green
            {0.85f, 0.65f, 0.30f},  // amber
            {0.45f, 0.45f, 0.70f},  // lavender
        };
        const int focusIdx = focusedForeignIndex_.load(std::memory_order_relaxed);
        constexpr float kForeignPosSmooth = 0.35f;

        // Draw audible radius spheres + sound waves for foreign sources (behind nodes)
        glDepthMask(GL_FALSE);
        for (int i = 0; i < fp.count; ++i) {
            const auto& fs = fp.sources[i];
            const auto color = kPalette[fs.colorIndex % 8];
            const int ri = std::clamp(i, 0, static_cast<int>(kMaxLinkedSources) - 1);

            // Per-frame exponential smoothing of foreign source positions
            auto& sp = foreignSmoothedPos_[ri];
            sp.x  += kForeignPosSmooth * (fs.x - sp.x);
            sp.y  += kForeignPosSmooth * (fs.y - sp.y);
            sp.z  += kForeignPosSmooth * (fs.z - sp.z);
            sp.lx += kForeignPosSmooth * (fs.lNodeX - sp.lx);
            sp.ly += kForeignPosSmooth * (fs.lNodeY - sp.ly);
            sp.lz += kForeignPosSmooth * (fs.lNodeZ - sp.lz);
            sp.rx += kForeignPosSmooth * (fs.rNodeX - sp.rx);
            sp.ry += kForeignPosSmooth * (fs.rNodeY - sp.ry);
            sp.rz += kForeignPosSmooth * (fs.rNodeZ - sp.rz);
            sp.sphereRadius += kForeignPosSmooth * (fs.sphereRadius - sp.sphereRadius);

            const glm::vec3 fPos(sp.x, sp.z, -sp.y);
            const float fsr = sp.sphereRadius * 0.25f;
            drawSphere(fPos, fsr, color, 0.03f);
            drawSphereWireframe(fsr, color, 0.02f, fPos);

            // Re-establish sphere shader for sound wave filled spheres
            sphereShader_->use();
            sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
            sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
            const glm::vec3 ld = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
            sphereShader_->setUniform("lightDir", ld.x, ld.y, ld.z);
            glBindVertexArray(vaoSphere_);

            // Smooth foreign source RMS and draw sound waves
            const float fRmsCoeff = (fs.inputRms > foreignSmoothedRms_[ri]) ? kRmsAttack : kRmsRelease;
            foreignSmoothedRms_[ri] += fRmsCoeff * (fs.inputRms - foreignSmoothedRms_[ri]);
            drawSoundWaves(fPos, fsr, foreignSmoothedRms_[ri], color, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);
        }
        glDepthMask(GL_TRUE);

        // Draw foreign source nodes on top
        for (int i = 0; i < fp.count; ++i) {
            const auto& fs = fp.sources[i];
            const auto color = kPalette[fs.colorIndex % 8];
            const int ri = std::clamp(i, 0, static_cast<int>(kMaxLinkedSources) - 1);
            const auto& sp = foreignSmoothedPos_[ri];
            const glm::vec3 fPos(sp.x, sp.z, -sp.y);
            drawSphere(fPos, 0.035f, color, 0.6f);
            // Highlight ring around focused foreign source
            if (fs.colorIndex == focusIdx)
                drawSphere(fPos, 0.050f, color, 0.25f);
            if (fs.stereoWidth > 0.0f) {
                const glm::vec3 fL(sp.lx, sp.lz, -sp.ly);
                const glm::vec3 fR(sp.rx, sp.rz, -sp.ry);
                drawSphere(fL, 0.028f, color * 0.8f, 0.45f);
                drawSphere(fR, 0.028f, color * 0.8f, 0.45f);
            }
        }
    }

    // End sphere/cone shader batch
    glBindVertexArray(0);

    // Draw trails — don't write depth to avoid occluding transparent geometry
    glDepthMask(GL_FALSE);
    {
        // Center trail only in mono mode; stereo mode uses L/R trails instead
        if (!stereoActive) {
            drawTrail(trailSource_, vaoTrailSource_, vboTrailSource_,
                      theme.glTrail, sourceOpacity * 0.6f, now);
        }

        if (stereoActive) {
            const float lDistFracT = std::clamp(glm::length(lNodePos) / sr, 0.0f, 1.0f);
            const float rDistFracT = std::clamp(glm::length(rNodePos) / sr, 0.0f, 1.0f);
            const float lTrailOpacity = (0.1f + 0.9f * (1.0f - lDistFracT)) * 0.5f;
            const float rTrailOpacity = (0.1f + 0.9f * (1.0f - rDistFracT)) * 0.5f;

            drawTrail(trailL_, vaoTrailL_, vboTrailL_,
                      theme.glStereoL, lTrailOpacity, now);
            drawTrail(trailR_, vaoTrailR_, vboTrailR_,
                      theme.glStereoR, rTrailOpacity, now);
        }

        // Listener trail — teal/aqua when walker is active
        if (walkerActive) {
            static const glm::vec3 kListenerTrailColor(0.30f, 0.66f, 0.66f); // aqua
            drawTrail(trailListener_, vaoTrailListener_, vboTrailListener_,
                      kListenerTrailColor, 0.5f, now);
        }
    }
    glDepthMask(GL_TRUE);

    // Billboard text labels — depth-test on (occluded by geometry), depth-write off
    glDepthMask(GL_FALSE);
    {
        // Own source label (use brightGold theme color)
        if (ownInstanceName_.isNotEmpty()) {
            const juce::Colour goldCol(theme.brightGold);
            const glm::vec3 goldVec(goldCol.getFloatRed(), goldCol.getFloatGreen(), goldCol.getFloatBlue());
            if (stereoActive) {
                drawBillboardLabel(lNodePos, ownInstanceName_.toStdString() + " L",
                                   goldVec, 0.85f);
                drawBillboardLabel(rNodePos, ownInstanceName_.toStdString() + " R",
                                   goldVec, 0.85f);
            } else {
                drawBillboardLabel(sourcePos, ownInstanceName_.toStdString(),
                                   goldVec, 0.85f);
            }
        }

        // Foreign source labels (use smoothed positions so labels track nodes)
        {
            static constexpr glm::vec3 kLabelPalette[8] = {
                {0.60f, 0.40f, 0.80f}, {0.30f, 0.70f, 0.50f},
                {0.80f, 0.45f, 0.30f}, {0.40f, 0.55f, 0.85f},
                {0.75f, 0.35f, 0.55f}, {0.50f, 0.70f, 0.35f},
                {0.85f, 0.65f, 0.30f}, {0.45f, 0.45f, 0.70f},
            };
            for (int i = 0; i < fp.count; ++i) {
                const auto& fs = fp.sources[i];
                if (fs.name[0] == '\0') continue;
                const int ri = std::clamp(i, 0, static_cast<int>(kMaxLinkedSources) - 1);
                const auto& sp = foreignSmoothedPos_[ri];
                const auto labelColor = kLabelPalette[fs.colorIndex % 8];
                const std::string name(fs.name);
                if (fs.stereoWidth > 0.0f) {
                    const glm::vec3 fL(sp.lx, sp.lz, -sp.ly);
                    const glm::vec3 fR(sp.rx, sp.rz, -sp.ry);
                    drawBillboardLabel(fL, name + " L", labelColor, 0.85f);
                    drawBillboardLabel(fR, name + " R", labelColor, 0.85f);
                } else {
                    const glm::vec3 fPos(sp.x, sp.z, -sp.y);
                    drawBillboardLabel(fPos, name, labelColor, 0.85f);
                }
            }
        }
    }
    glDepthMask(GL_TRUE);

    // Cache projected source position for hit-testing on mouse events
    projectedSourcePos_ = projectToScreen(sourcePos);

}

// ---------------------------------------------------------------------------
// openGLContextClosing — delete GPU resources
// ---------------------------------------------------------------------------
void XYZPanGLView::openGLContextClosing()
{
    lineShader_.reset();
    colorLineShader_.reset();
    sphereShader_.reset();
    trailShader_.reset();
    if (vaoRoom_)   { glDeleteVertexArrays(1, &vaoRoom_);   vaoRoom_   = 0; }
    if (vboRoom_)   { glDeleteBuffers(1, &vboRoom_);        vboRoom_   = 0; }
    if (vaoGrid_)   { glDeleteVertexArrays(1, &vaoGrid_);   vaoGrid_   = 0; }
    if (vboGrid_)   { glDeleteBuffers(1, &vboGrid_);        vboGrid_   = 0; }
    if (vaoSphere_) { glDeleteVertexArrays(1, &vaoSphere_); vaoSphere_ = 0; }
    if (vboSphere_) { glDeleteBuffers(1, &vboSphere_);      vboSphere_ = 0; }
    if (iboSphere_) { glDeleteBuffers(1, &iboSphere_);      iboSphere_ = 0; }
    if (vaoSphereWire_) { glDeleteVertexArrays(1, &vaoSphereWire_); vaoSphereWire_ = 0; }
    if (iboSphereWire_) { glDeleteBuffers(1, &iboSphereWire_);      iboSphereWire_ = 0; }
    if (vaoSphereRings_) { glDeleteVertexArrays(1, &vaoSphereRings_); vaoSphereRings_ = 0; }
    if (iboSphereRings_) { glDeleteBuffers(1, &iboSphereRings_);      iboSphereRings_ = 0; }
    if (vaoCone_)   { glDeleteVertexArrays(1, &vaoCone_);   vaoCone_   = 0; }
    if (vboCone_)   { glDeleteBuffers(1, &vboCone_);        vboCone_   = 0; }
    if (iboCone_)   { glDeleteBuffers(1, &iboCone_);        iboCone_   = 0; }
    if (vaoArrow2D_) { glDeleteVertexArrays(1, &vaoArrow2D_); vaoArrow2D_ = 0; }
    if (vboArrow2D_) { glDeleteBuffers(1, &vboArrow2D_);      vboArrow2D_ = 0; }

    if (vaoTrailSource_) { glDeleteVertexArrays(1, &vaoTrailSource_); vaoTrailSource_ = 0; }
    if (vboTrailSource_) { glDeleteBuffers(1, &vboTrailSource_);      vboTrailSource_ = 0; }
    if (vaoTrailL_)      { glDeleteVertexArrays(1, &vaoTrailL_);      vaoTrailL_      = 0; }
    if (vboTrailL_)      { glDeleteBuffers(1, &vboTrailL_);           vboTrailL_      = 0; }
    if (vaoTrailR_)        { glDeleteVertexArrays(1, &vaoTrailR_);        vaoTrailR_        = 0; }
    if (vboTrailR_)        { glDeleteBuffers(1, &vboTrailR_);           vboTrailR_        = 0; }
    if (vaoTrailListener_) { glDeleteVertexArrays(1, &vaoTrailListener_); vaoTrailListener_ = 0; }
    if (vboTrailListener_) { glDeleteBuffers(1, &vboTrailListener_);      vboTrailListener_ = 0; }

    textShader_.reset();
    if (vaoText_) { glDeleteVertexArrays(1, &vaoText_); vaoText_ = 0; }
    if (vboText_) { glDeleteBuffers(1, &vboText_); vboText_ = 0; }
    for (auto& [key, cached] : textTextureCache_)
        glDeleteTextures(1, &cached.textureId);
    textTextureCache_.clear();

}

// ---------------------------------------------------------------------------
// paint — JUCE overlay composited on top of GL (instance list HUD)
// ---------------------------------------------------------------------------
void XYZPanGLView::paint(juce::Graphics& g)
{
    // Palette for instance list entries
    static const juce::Colour kLabelPalette[8] = {
        juce::Colour::fromFloatRGBA(0.60f, 0.40f, 0.80f, 1.0f),  // purple
        juce::Colour::fromFloatRGBA(0.30f, 0.70f, 0.50f, 1.0f),  // teal-green
        juce::Colour::fromFloatRGBA(0.80f, 0.45f, 0.30f, 1.0f),  // burnt orange
        juce::Colour::fromFloatRGBA(0.40f, 0.55f, 0.85f, 1.0f),  // steel blue
        juce::Colour::fromFloatRGBA(0.75f, 0.35f, 0.55f, 1.0f),  // rose
        juce::Colour::fromFloatRGBA(0.50f, 0.70f, 0.35f, 1.0f),  // olive green
        juce::Colour::fromFloatRGBA(0.85f, 0.65f, 0.30f, 1.0f),  // amber
        juce::Colour::fromFloatRGBA(0.45f, 0.45f, 0.70f, 1.0f),  // lavender
    };

    const auto fp = foreignBridge_.read();

    // Instance list overlay — top-left corner of GL view
    if (showInstanceList_) {
        const int focusIdx = focusedForeignIndex_.load(std::memory_order_relaxed);
        instanceListHitBoxes_.clear();

        const int listX = 8;
        int listY = 8;
        const int rowH = 20;
        const int focusedRowH = 26;
        const int listW = 140;

        // "Self" entry
        {
            const bool isFocused = (focusIdx < 0);
            const int rh = isFocused ? focusedRowH : rowH;
            const float fontSize = isFocused ? 13.0f : 11.0f;
            g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));

            ColorTheme theme;
            {
                const juce::SpinLock::ScopedLockType lock(customizeLock_);
                theme = glTheme_;
            }
            juce::Colour col = juce::Colour(theme.brightGold);
            if (isFocused)
                col = col.brighter(0.3f);
            g.setColour(col.withAlpha(isFocused ? 1.0f : 0.7f));
            juce::String selfName = ownInstanceName_.isNotEmpty() ? ownInstanceName_ : "Self";
            g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) + " " + selfName,
                       listX, listY, listW, rh, juce::Justification::centredLeft, false);

            // Glow effect for focused entry
            if (isFocused) {
                g.setColour(col.withAlpha(0.15f));
                g.fillRoundedRectangle(static_cast<float>(listX - 2), static_cast<float>(listY - 1),
                                       static_cast<float>(listW + 4), static_cast<float>(rh + 2), 3.0f);
            }

            instanceListHitBoxes_.push_back({{listX - 4, listY - 2, listW + 8, rh + 4}, -1});
            listY += rh + 2;
        }

        // Foreign source entries
        for (int i = 0; i < fp.count; ++i) {
            const auto& fs = fp.sources[i];
            const bool isFocused = (fs.colorIndex == focusIdx);
            const int rh = isFocused ? focusedRowH : rowH;
            const float fontSize = isFocused ? 13.0f : 11.0f;
            g.setFont(juce::Font(juce::FontOptions(fontSize, juce::Font::bold)));

            juce::String name(fs.name);
            if (name.isEmpty()) name = "Source " + juce::String(i + 1);

            juce::Colour col = kLabelPalette[fs.colorIndex % 8];
            if (isFocused)
                col = col.brighter(0.3f);
            g.setColour(col.withAlpha(isFocused ? 1.0f : 0.7f));
            g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) + " " + name,
                       listX, listY, listW, rh, juce::Justification::centredLeft, false);

            if (isFocused) {
                g.setColour(col.withAlpha(0.15f));
                g.fillRoundedRectangle(static_cast<float>(listX - 2), static_cast<float>(listY - 1),
                                       static_cast<float>(listW + 4), static_cast<float>(rh + 2), 3.0f);
            }

            instanceListHitBoxes_.push_back({{listX - 4, listY - 2, listW + 8, rh + 4}, i});
            listY += rh + 2;
        }

        // Placeholder entries when no linked instances (for UI testing)
        if (fp.count == 0) {
            g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            for (int i = 0; i < 2; ++i) {
                g.setColour(kLabelPalette[i].withAlpha(0.3f));
                g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) + " Source " + juce::String(i + 1),
                           listX, listY, listW, rowH, juce::Justification::centredLeft, false);
                listY += rowH + 2;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Mouse: Down
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseDown(const juce::MouseEvent& e)
{
    lastDragPos_ = e.getPosition();

    // Hit-test instance list overlay first
    if (showInstanceList_) {
        const auto pos = e.getPosition();
        for (const auto& entry : instanceListHitBoxes_) {
            if (entry.bounds.contains(pos)) {
                if (onInstanceClicked)
                    onInstanceClicked(entry.linkedIndex);
                return;
            }
        }
    }

    if (isNearSourceNode(e.getPosition())) {
        isDraggingSource_ = true;
        isDraggingCamera_ = false;
        setMouseCursor(juce::MouseCursor(juce::MouseCursor::DraggingHandCursor));
    } else {
        isDraggingSource_ = false;
        isDraggingCamera_ = true;
        setMouseCursor(juce::MouseCursor(juce::MouseCursor::NoCursor));
        dragAnchorScreen_ = e.getScreenPosition();
    }
}

// ---------------------------------------------------------------------------
// Mouse: Double-click — rename own instance in overlay list
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!showInstanceList_) return;

    const auto pos = e.getPosition();
    for (const auto& entry : instanceListHitBoxes_) {
        if (entry.linkedIndex == -1 && entry.bounds.contains(pos)) {
            // Show inline text editor over the Self entry
            renameEditor_ = std::make_unique<juce::TextEditor>();
            renameEditor_->setBounds(entry.bounds.getX(), entry.bounds.getY(),
                                     entry.bounds.getWidth(), entry.bounds.getHeight());
            renameEditor_->setText(ownInstanceName_.isNotEmpty() ? ownInstanceName_ : "Self",
                                   false);
            renameEditor_->selectAll();
            renameEditor_->setColour(juce::TextEditor::backgroundColourId,
                                     juce::Colours::black.withAlpha(0.7f));
            renameEditor_->setColour(juce::TextEditor::textColourId,
                                     juce::Colours::white);
            renameEditor_->setColour(juce::TextEditor::outlineColourId,
                                     juce::Colours::transparentBlack);
            renameEditor_->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));

            auto finishRename = [this] {
                if (renameEditor_) {
                    auto newName = renameEditor_->getText().trim();
                    if (newName.isNotEmpty()) {
                        ownInstanceName_ = newName;
                        if (onInstanceRenamed)
                            onInstanceRenamed(newName);
                    }
                    removeChildComponent(renameEditor_.get());
                    renameEditor_.reset();
                    repaint();
                }
            };

            renameEditor_->onReturnKey = finishRename;
            renameEditor_->onFocusLost = finishRename;

            addAndMakeVisible(*renameEditor_);
            renameEditor_->grabKeyboardFocus();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Mouse: Drag
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseDrag(const juce::MouseEvent& e)
{
    const juce::Point<int> currentPos = e.getPosition();
    juce::Point<int> delta = currentPos - lastDragPos_;
    lastDragPos_ = currentPos;

    // Camera drag: compute delta from screen anchor and warp cursor back
    // so the mouse never hits screen edges.
    if (isDraggingCamera_) {
        const auto screenPos = e.getScreenPosition();
        delta = screenPos - dragAnchorScreen_;
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition(
            dragAnchorScreen_.toFloat());
        lastDragPos_ = getLocalPoint(nullptr, dragAnchorScreen_);
    }

    if (isDraggingSource_) {
        float dX = 0.0f, dY = 0.0f, dZ = 0.0f;
        computeDragDelta(delta, dX, dY, dZ);

        // Read current APVTS parameter values
        const float curX = [&] { auto* a = apvts_.getRawParameterValue(kParamX); return a ? a->load() : 0.0f; }();
        const float curY = [&] { auto* a = apvts_.getRawParameterValue(kParamY); return a ? a->load() : 1.0f; }();
        const float curZ = [&] { auto* a = apvts_.getRawParameterValue(kParamZ); return a ? a->load() : 0.0f; }();

        const float newX = std::clamp(curX + dX, -1.0f, 1.0f);
        const float newY = std::clamp(curY + dY, -1.0f, 1.0f);
        const float newZ = std::clamp(curZ + dZ, -1.0f, 1.0f);

        // NEVER write APVTS from GL/render thread — post to message thread.
        // Capture alive flag (shared_ptr) so lambda is safe if GLView is destroyed.
        juce::AudioProcessorValueTreeState* apvtsPtr = &apvts_;
        auto alive = glAlive_;
        juce::MessageManager::callAsync(
            [apvtsPtr, alive, newX, newY, newZ]() {
                if (!alive->load(std::memory_order_acquire)) return;
                if (auto* p = apvtsPtr->getParameter(kParamX))
                    p->setValueNotifyingHost(apvtsPtr->getParameterRange(kParamX).convertTo0to1(newX));
                if (auto* p = apvtsPtr->getParameter(kParamY))
                    p->setValueNotifyingHost(apvtsPtr->getParameterRange(kParamY).convertTo0to1(newY));
                if (auto* p = apvtsPtr->getParameter(kParamZ))
                    p->setValueNotifyingHost(apvtsPtr->getParameterRange(kParamZ).convertTo0to1(newZ));
            });
    } else if (headFollowsActive_) {
        // Head-follows drag: use quaternion-based camera drag (no Euler round-trip),
        // then push extracted Euler to APVTS for the audio engine.
        camera_.applyMouseDrag(static_cast<float>(delta.x),
                               static_cast<float>(delta.y));

        const float newYaw   = camera_.yaw;
        const float newPitch = camera_.pitch;
        auto* apvtsPtr = &apvts_;
        auto alive = glAlive_;
        juce::MessageManager::callAsync([apvtsPtr, alive, newYaw, newPitch]() {
            if (!alive->load(std::memory_order_acquire)) return;
            if (auto* p = apvtsPtr->getParameter("listener_yaw"))
                p->setValueNotifyingHost(p->convertTo0to1(newYaw));
            if (auto* p = apvtsPtr->getParameter("listener_pitch"))
                p->setValueNotifyingHost(p->convertTo0to1(newPitch));
        });
    } else {
        // Camera orbit — detect if drag exits a snap view
        const bool wasSnapped = camera_.activeSnap != Camera::SnapView::Orbit;
        camera_.applyMouseDrag(static_cast<float>(delta.x),
                               static_cast<float>(delta.y));
        if (wasSnapped && onSnapExited)
            onSnapExited();
    }
}

// ---------------------------------------------------------------------------
// Mouse: Up
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseUp(const juce::MouseEvent& /*e*/)
{
    isDraggingSource_ = false;
    isDraggingCamera_ = false;
    setMouseCursor(juce::MouseCursor(juce::MouseCursor::NormalCursor));
}

// ---------------------------------------------------------------------------
// Mouse: Wheel (scroll to zoom)
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseWheelMove(const juce::MouseEvent& /*e*/,
                                    const juce::MouseWheelDetails& wheel)
{
    constexpr float kZoomSpeed = 0.5f;
    constexpr float kMinDist   = 0.002f;
    constexpr float kMaxDist   = 10.0f;

    camera_.dist = std::clamp(camera_.dist - wheel.deltaY * kZoomSpeed,
                              kMinDist, kMaxDist);
}

// ---------------------------------------------------------------------------
// Mouse: Move (hover detection for source node highlight)
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseMove(const juce::MouseEvent& e)
{
    const bool wasHovered = isSourceHovered_;
    isSourceHovered_ = isNearSourceNode(e.getPosition());

    if (isSourceHovered_)
        setMouseCursor(juce::MouseCursor(juce::MouseCursor::PointingHandCursor));
    else
        setMouseCursor(juce::MouseCursor(juce::MouseCursor::NormalCursor));

    // Repaint only when hover state changes (to update node color)
    if (wasHovered != isSourceHovered_)
        repaint();
}

// ---------------------------------------------------------------------------
// setSnapView
// ---------------------------------------------------------------------------
void XYZPanGLView::setSnapView(SnapView v)
{
    switch (v) {
        case SnapView::TopDown: camera_.setSnapTopDown(); break;
        case SnapView::Side:    camera_.setSnapSide();    break;
        case SnapView::Front:   camera_.setSnapFront();   break;
        case SnapView::Orbit:   camera_.setOrbit();       break;
    }
}

// ---------------------------------------------------------------------------
// compileShaders
// ---------------------------------------------------------------------------
void XYZPanGLView::compileShaders()
{
    // Line shader (room wireframe + floor grid)
    lineShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!lineShader_->addVertexShader(kLineVertShader) ||
        !lineShader_->addFragmentShader(kLineFragShader) ||
        !lineShader_->link()) {
        lineShader_.reset();
        jassertfalse;
        return;
    }

    // Colored-line shader (room wireframe with per-vertex axis colors)
    colorLineShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!colorLineShader_->addVertexShader(kColorLineVertShader) ||
        !colorLineShader_->addFragmentShader(kColorLineFragShader) ||
        !colorLineShader_->link()) {
        colorLineShader_.reset();
        jassertfalse;
        return;
    }

    // Sphere shader (listener + source nodes)
    sphereShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!sphereShader_->addVertexShader(kSphereVertShader) ||
        !sphereShader_->addFragmentShader(kSphereFragShader) ||
        !sphereShader_->link()) {
        sphereShader_.reset();
        jassertfalse;
        return;
    }

    // Trail shader (fading position trails)
    trailShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!trailShader_->addVertexShader(kTrailVertShader) ||
        !trailShader_->addFragmentShader(kTrailFragShader) ||
        !trailShader_->link()) {
        trailShader_.reset();
        jassertfalse;
        return;
    }

    // Text billboard shader (name labels)
    textShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!textShader_->addVertexShader(kTextVertShader) ||
        !textShader_->addFragmentShader(kTextFragShader) ||
        !textShader_->link()) {
        textShader_.reset();
        jassertfalse;
        return;
    }

}

// ---------------------------------------------------------------------------
// uploadLineVAO — create VAO+VBO for a flat [x,y,z] vertex list
// ---------------------------------------------------------------------------
void XYZPanGLView::uploadLineVAO(GLuint& vao, GLuint& vbo,
                                   const std::vector<float>& vertices)
{
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);

    // attribute 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// uploadColorLineVAO — interleaved [x,y,z, r,g,b] per vertex
// ---------------------------------------------------------------------------
void XYZPanGLView::uploadColorLineVAO(GLuint& vao, GLuint& vbo,
                                        const std::vector<float>& vertices)
{
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);

    const GLsizei stride = 6 * sizeof(float);
    // attribute 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    // attribute 1: vertColor (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// drawColorLines — draw per-vertex colored lines with the color-line shader
// ---------------------------------------------------------------------------
void XYZPanGLView::drawColorLines(GLuint vao, int vertexCount,
                                    float opacity, const glm::mat4& modelMatrix)
{
    if (!colorLineShader_ || vao == 0 || vertexCount == 0) return;

    colorLineShader_->use();
    colorLineShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    colorLineShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    colorLineShader_->setUniformMat4("model",      glm::value_ptr(modelMatrix), 1, GL_FALSE);
    colorLineShader_->setUniform("opacity", opacity);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, vertexCount);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// uploadSphereVAO — interleaved pos(3)+normal(3) with index buffer
// ---------------------------------------------------------------------------
void XYZPanGLView::uploadSphereVAO()
{
    if (vaoSphere_) glDeleteVertexArrays(1, &vaoSphere_);
    if (vboSphere_) glDeleteBuffers(1, &vboSphere_);
    if (iboSphere_) glDeleteBuffers(1, &iboSphere_);

    glGenVertexArrays(1, &vaoSphere_);
    glGenBuffers(1, &vboSphere_);
    glGenBuffers(1, &iboSphere_);

    glBindVertexArray(vaoSphere_);

    // VBO: interleaved position(3) + normal(3)
    glBindBuffer(GL_ARRAY_BUFFER, vboSphere_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sphereVerts_.size() * sizeof(float)),
                 sphereVerts_.data(), GL_STATIC_DRAW);

    // IBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboSphere_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(sphereIdx_.size() * sizeof(unsigned)),
                 sphereIdx_.data(), GL_STATIC_DRAW);

    const GLsizei stride = 6 * sizeof(float);
    // attribute 0: position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    // attribute 1: normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // --- Wireframe VAO: shares vboSphere_ data, uses separate line indices ---
    if (vaoSphereWire_) glDeleteVertexArrays(1, &vaoSphereWire_);
    if (iboSphereWire_) glDeleteBuffers(1, &iboSphereWire_);

    auto wireIdx = buildSphereWireframe(16, 16);
    sphereWireIndexCount_ = static_cast<int>(wireIdx.size());

    glGenVertexArrays(1, &vaoSphereWire_);
    glGenBuffers(1, &iboSphereWire_);

    glBindVertexArray(vaoSphereWire_);

    // Bind the SAME sphere VBO (interleaved pos+normal, stride=24)
    glBindBuffer(GL_ARRAY_BUFFER, vboSphere_);

    // Only need position (attribute 0) — lineShader_ uses position only
    const GLsizei wireStride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, wireStride, nullptr);

    // Upload wireframe index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboSphereWire_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(wireIdx.size() * sizeof(unsigned)),
                 wireIdx.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // --- Latitude-only rings VAO: shares vboSphere_ data, latitude lines only ---
    if (vaoSphereRings_) glDeleteVertexArrays(1, &vaoSphereRings_);
    if (iboSphereRings_) glDeleteBuffers(1, &iboSphereRings_);

    auto ringsIdx = buildSphereLatitudeRings(16, 16);
    sphereRingsIndexCount_ = static_cast<int>(ringsIdx.size());

    glGenVertexArrays(1, &vaoSphereRings_);
    glGenBuffers(1, &iboSphereRings_);

    glBindVertexArray(vaoSphereRings_);
    glBindBuffer(GL_ARRAY_BUFFER, vboSphere_);

    const GLsizei ringsStride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, ringsStride, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboSphereRings_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(ringsIdx.size() * sizeof(unsigned)),
                 ringsIdx.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// drawLines — draw a line-VAO with the line shader
// ---------------------------------------------------------------------------
void XYZPanGLView::drawLines(GLuint vao, int vertexCount,
                              const glm::vec3& color, float opacity,
                              const glm::mat4& modelMatrix)
{
    if (!lineShader_ || vao == 0 || vertexCount == 0) return;

    lineShader_->use();

    // Set matrix uniforms via raw GL calls (getUniformIDFromName returns -1 if not found,
    // glUniformMatrix4fv with location -1 is a no-op in core GL)
    lineShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("model",      glm::value_ptr(modelMatrix), 1, GL_FALSE);
    lineShader_->setUniform("lineColor", color.r, color.g, color.b);
    lineShader_->setUniform("opacity",   opacity);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, vertexCount);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// drawSphere — draw a sphere at worldPos with given radius, color, opacity
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSphere(const glm::vec3& position, float radius,
                               const glm::vec3& color, float opacity)
{
    if (!sphereShader_ || vaoSphere_ == 0 || sphereIndexCount_ == 0) return;

    const glm::mat4 model = glm::scale(
        glm::translate(glm::mat4(1.0f), position),
        glm::vec3(radius));

    // Shader bind, projection/view/lightDir already set by batch setup block in renderOpenGL()
    sphereShader_->setUniformMat4("model",     glm::value_ptr(model), 1, GL_FALSE);
    sphereShader_->setUniform("nodeColor", color.r, color.g, color.b);
    sphereShader_->setUniform("opacity",   opacity);
    sphereShader_->setUniform("edgeFade",  0.0f);
    glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, nullptr);
}

// ---------------------------------------------------------------------------
// drawSphereWireframe — lat/long grid via GL_LINES using lineShader_
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSphereWireframe(float radius, const glm::vec3& color, float opacity,
                                        const glm::vec3& position)
{
    if (!lineShader_ || vaoSphereWire_ == 0 || sphereWireIndexCount_ == 0) return;

    const glm::mat4 model = glm::scale(
        glm::translate(glm::mat4(1.0f), position), glm::vec3(radius));

    lineShader_->use();
    lineShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("model",      glm::value_ptr(model),       1, GL_FALSE);
    lineShader_->setUniform("lineColor", color.r, color.g, color.b);
    lineShader_->setUniform("opacity",   opacity);

    glBindVertexArray(vaoSphereWire_);
    glDrawElements(GL_LINES, sphereWireIndexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// drawSphereRings — latitude rings only (no longitude meridians)
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSphereRings(float radius, const glm::vec3& color, float opacity,
                                    const glm::vec3& position)
{
    if (!lineShader_ || vaoSphereRings_ == 0 || sphereRingsIndexCount_ == 0) return;

    const glm::mat4 model = glm::scale(
        glm::translate(glm::mat4(1.0f), position), glm::vec3(radius));

    lineShader_->use();
    lineShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("model",      glm::value_ptr(model),       1, GL_FALSE);
    lineShader_->setUniform("lineColor", color.r, color.g, color.b);
    lineShader_->setUniform("opacity",   opacity);

    glBindVertexArray(vaoSphereRings_);
    glDrawElements(GL_LINES, sphereRingsIndexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// drawSoundWaves — expanding filled sphere pulses radiating outward from source.
// Each wave is a semi-transparent filled sphere. Opacity fades from baseOpacity
// near the source to 0 at the audible radius boundary. Multiple overlapping
// spheres visually stack, making the center appear denser.
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSoundWaves(const glm::vec3& center, float sphereRadius,
                                   float inputRms, const glm::vec3& color,
                                   double nowSeconds, float intensity,
                                   float baseOpacity, float speed, int numWaves)
{
    if (inputRms < 0.001f || sphereRadius < 0.001f || intensity < 0.001f
        || baseOpacity < 0.0001f || numWaves < 1) return;

    // Volume-based opacity multiplier: 0dB (rms=1.0) → 2x, -6dB → 1x, -12dB → 0.5x
    const float rmsMul = std::clamp(inputRms * 2.0f, 0.0f, 2.0f);

    const float waveSpacing = 1.0f / static_cast<float>(numWaves);

    for (int i = 0; i < numWaves; ++i) {
        float phase = static_cast<float>(std::fmod(nowSeconds * speed + i * waveSpacing, 1.0));

        float waveRadius = phase * sphereRadius;
        if (waveRadius < 0.005f) continue;

        // Opacity: starts at baseOpacity near center, fades linearly to 0 at boundary.
        // Volume (rmsMul) scales opacity — louder = more visible, quieter = more transparent
        float fadeIn  = std::clamp(phase * 5.0f, 0.0f, 1.0f);  // quick ramp-in
        float fadeOut = 1.0f - phase;                             // linear fade to edge
        float waveOpacity = fadeIn * fadeOut * baseOpacity * intensity * rmsMul;

        if (waveOpacity < 0.001f) continue;

        // Inline sphere draw with edgeFade=1.0 for soft-edged wave shells
        const glm::mat4 model = glm::scale(
            glm::translate(glm::mat4(1.0f), center),
            glm::vec3(waveRadius));
        sphereShader_->setUniformMat4("model", glm::value_ptr(model), 1, GL_FALSE);
        sphereShader_->setUniform("nodeColor", color.r, color.g, color.b);
        sphereShader_->setUniform("opacity",   waveOpacity);
        sphereShader_->setUniform("edgeFade",  1.0f);
        glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, nullptr);
    }
}

// ---------------------------------------------------------------------------
// drawSphereWithModel — draw sphere VAO with arbitrary model matrix
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSphereWithModel(const glm::mat4& model,
                                         const glm::vec3& color, float opacity)
{
    if (!sphereShader_ || vaoSphere_ == 0 || sphereIndexCount_ == 0) return;

    // Shader bind, projection/view/lightDir already set by batch setup block in renderOpenGL()
    sphereShader_->setUniformMat4("model",     glm::value_ptr(model), 1, GL_FALSE);
    sphereShader_->setUniform("nodeColor", color.r, color.g, color.b);
    sphereShader_->setUniform("opacity",   opacity);
    sphereShader_->setUniform("edgeFade",  0.0f);
    glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, nullptr);
}

// ---------------------------------------------------------------------------
// uploadConeVAO — interleaved pos(3)+normal(3) with index buffer
// ---------------------------------------------------------------------------
void XYZPanGLView::uploadConeVAO()
{
    if (vaoCone_) glDeleteVertexArrays(1, &vaoCone_);
    if (vboCone_) glDeleteBuffers(1, &vboCone_);
    if (iboCone_) glDeleteBuffers(1, &iboCone_);

    glGenVertexArrays(1, &vaoCone_);
    glGenBuffers(1, &vboCone_);
    glGenBuffers(1, &iboCone_);

    glBindVertexArray(vaoCone_);

    glBindBuffer(GL_ARRAY_BUFFER, vboCone_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(coneVerts_.size() * sizeof(float)),
                 coneVerts_.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboCone_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(coneIdx_.size() * sizeof(unsigned)),
                 coneIdx_.data(), GL_STATIC_DRAW);

    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// drawCone — draw cone VAO with arbitrary model matrix (reuses sphere shader)
// ---------------------------------------------------------------------------
void XYZPanGLView::drawCone(const glm::mat4& model,
                              const glm::vec3& color, float opacity)
{
    if (!sphereShader_ || vaoCone_ == 0 || coneIndexCount_ == 0) return;

    // Shader bind, projection/view/lightDir already set by batch setup block in renderOpenGL().
    // Cone uses a different VAO, so switch to it and restore sphere VAO afterwards.
    glBindVertexArray(vaoCone_);
    sphereShader_->setUniformMat4("model",     glm::value_ptr(model), 1, GL_FALSE);
    sphereShader_->setUniform("nodeColor", color.r, color.g, color.b);
    sphereShader_->setUniform("opacity",   opacity);
    sphereShader_->setUniform("edgeFade",  0.0f);
    glDrawElements(GL_TRIANGLES, coneIndexCount_, GL_UNSIGNED_INT, nullptr);
    // Restore sphere VAO since cone is drawn mid-batch
    glBindVertexArray(vaoSphere_);
}

// ---------------------------------------------------------------------------
// uploadArrow2DVAO — flat 2D arrow (shaft + arrowhead) as GL_TRIANGLES
// Arrow points along +Y in local space, centered at origin base.
// ---------------------------------------------------------------------------
void XYZPanGLView::uploadArrow2DVAO()
{
    if (vaoArrow2D_) glDeleteVertexArrays(1, &vaoArrow2D_);
    if (vboArrow2D_) glDeleteBuffers(1, &vboArrow2D_);

    // Arrow dimensions (unit-ish, scaled by model matrix at draw time)
    constexpr float shaftW  = 0.25f;   // half-width of the shaft
    constexpr float shaftH  = 0.6f;    // shaft height (from base)
    constexpr float headW   = 0.5f;    // half-width of arrowhead base
    constexpr float totalH  = 1.0f;    // tip of the arrowhead

    // All verts in XY plane (Z=0).  6 triangles = shaft quad (2 tri) + arrowhead (1 tri)
    // Shaft: two triangles forming a rectangle
    // Arrowhead: one triangle on top
    const float verts[] = {
        // Shaft triangle 1
        -shaftW, 0.0f,    0.0f,
         shaftW, 0.0f,    0.0f,
         shaftW, shaftH,  0.0f,
        // Shaft triangle 2
        -shaftW, 0.0f,    0.0f,
         shaftW, shaftH,  0.0f,
        -shaftW, shaftH,  0.0f,
        // Arrowhead triangle
        -headW,  shaftH,  0.0f,
         headW,  shaftH,  0.0f,
         0.0f,   totalH,  0.0f,
    };

    arrow2DVertexCount_ = 9;

    glGenVertexArrays(1, &vaoArrow2D_);
    glGenBuffers(1, &vboArrow2D_);

    glBindVertexArray(vaoArrow2D_);
    glBindBuffer(GL_ARRAY_BUFFER, vboArrow2D_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// drawArrow2D — flat 2D arrow using line shader (unlit, solid color)
// ---------------------------------------------------------------------------
void XYZPanGLView::drawArrow2D(const glm::mat4& model,
                                 const glm::vec3& color, float opacity)
{
    if (!lineShader_ || vaoArrow2D_ == 0 || arrow2DVertexCount_ == 0) return;

    lineShader_->use();
    lineShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("model",      glm::value_ptr(model),       1, GL_FALSE);
    lineShader_->setUniform("lineColor", color.r, color.g, color.b);
    lineShader_->setUniform("opacity",   opacity);

    glBindVertexArray(vaoArrow2D_);
    glDrawArrays(GL_TRIANGLES, 0, arrow2DVertexCount_);

    // Restore sphere shader + sphere VAO for the ongoing batch
    sphereShader_->use();
    sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    const glm::vec3 ld = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
    sphereShader_->setUniform("lightDir", ld.x, ld.y, ld.z);
    glBindVertexArray(vaoSphere_);
}

// ---------------------------------------------------------------------------
// drawTrail — draw a fading trail from a TrailBuffer
// ---------------------------------------------------------------------------
void XYZPanGLView::drawTrail(TrailBuffer& trail, GLuint vao, GLuint vbo,
                              const glm::vec3& color, float baseOpacity,
                              double nowSeconds)
{
    if (!trailShader_ || vao == 0) return;

    const int vertCount = trail.fillVertexData(trailVertexStaging_.data(), nowSeconds);
    if (vertCount < 2) return;

    // Upload active portion to VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(vertCount * 4 * sizeof(float)),
                    trailVertexStaging_.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    trailShader_->use();
    trailShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    trailShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    trailShader_->setUniform("trailColor",  color.r, color.g, color.b);
    trailShader_->setUniform("baseOpacity", baseOpacity);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINE_STRIP, 0, vertCount);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// projectToScreen — world to screen (pixel) coordinates
// ---------------------------------------------------------------------------
glm::vec2 XYZPanGLView::projectToScreen(const glm::vec3& worldPos) const
{
    const glm::vec4 clip = projMatrix_ * viewMatrix_ * glm::vec4(worldPos, 1.0f);
    if (std::abs(clip.w) < 1e-6f) return glm::vec2(-9999.0f, -9999.0f);

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const float px = (ndc.x * 0.5f + 0.5f) * static_cast<float>(getWidth());
    const float py = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(getHeight());
    return glm::vec2(px, py);
}

// ---------------------------------------------------------------------------
// isNearSourceNode — check if mouse is within hit radius of projected source
// ---------------------------------------------------------------------------
bool XYZPanGLView::isNearSourceNode(const juce::Point<int>& mousePos) const
{
    constexpr float kHitRadius = 12.0f;
    const float dx = static_cast<float>(mousePos.x) - projectedSourcePos_.x;
    const float dy = static_cast<float>(mousePos.y) - projectedSourcePos_.y;
    return (dx * dx + dy * dy) <= (kHitRadius * kHitRadius);
}

// ---------------------------------------------------------------------------
// uploadTextQuadVAO — unit quad for billboard text labels
// ---------------------------------------------------------------------------
void XYZPanGLView::uploadTextQuadVAO()
{
    // 6 vertices, 4 floats each: [x, y, u, v]
    // V flipped so JUCE Image (y=0 top) maps correctly to GL texture (y=0 bottom)
    const float verts[] = {
        -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.0f, 0.0f,
    };

    glGenVertexArrays(1, &vaoText_);
    glGenBuffers(1, &vboText_);
    glBindVertexArray(vaoText_);
    glBindBuffer(GL_ARRAY_BUFFER, vboText_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, reinterpret_cast<void*>(8));
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// getOrCreateTextTexture — rasterise text string to GL texture via JUCE Image
// ---------------------------------------------------------------------------
XYZPanGLView::CachedTextTexture XYZPanGLView::getOrCreateTextTexture(const std::string& text)
{
    auto it = textTextureCache_.find(text);
    if (it != textTextureCache_.end())
        return it->second;

    const auto font = juce::Font(juce::FontOptions(32.0f, juce::Font::bold));
    const juce::String jText(text);
    const int tw = font.getStringWidth(jText) + 8;  // padding
    const int th = 40;

    juce::Image image(juce::Image::ARGB, tw, th, true);
    {
        juce::Graphics g(image);
        g.setColour(juce::Colours::white);
        g.setFont(font);
        g.drawText(jText, 0, 0, tw, th, juce::Justification::centred, false);
    }

    GLuint texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::readOnly);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, bitmap.data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    CachedTextTexture cached{texId, static_cast<float>(tw) / static_cast<float>(th)};
    textTextureCache_[text] = cached;
    return cached;
}

// ---------------------------------------------------------------------------
// drawBillboardLabel — camera-facing textured quad at a world position
// ---------------------------------------------------------------------------
void XYZPanGLView::drawBillboardLabel(const glm::vec3& worldPos,
                                       const std::string& text,
                                       const glm::vec3& color,
                                       float opacity)
{
    if (!textShader_ || vaoText_ == 0 || text.empty()) return;

    auto cached = getOrCreateTextTexture(text);
    if (cached.textureId == 0) return;

    // Billboard: extract camera right/up from view matrix
    const glm::vec3 camRight(viewMatrix_[0][0], viewMatrix_[1][0], viewMatrix_[2][0]);
    const glm::vec3 camUp(viewMatrix_[0][1], viewMatrix_[1][1], viewMatrix_[2][1]);

    // Position label above the node
    const glm::vec3 labelPos = worldPos + glm::vec3(0.0f, 0.07f, 0.0f);

    const float quadHeight = 0.04f;
    const float quadWidth  = quadHeight * cached.aspectRatio;

    // Build billboard model matrix
    glm::mat4 model(1.0f);
    model[0] = glm::vec4(camRight * quadWidth, 0.0f);
    model[1] = glm::vec4(camUp * quadHeight, 0.0f);
    model[2] = glm::vec4(glm::cross(camRight, camUp), 0.0f);
    model[3] = glm::vec4(labelPos, 1.0f);

    textShader_->use();
    textShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    textShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    textShader_->setUniformMat4("model",      glm::value_ptr(model), 1, GL_FALSE);
    textShader_->setUniform("textColor", color.r, color.g, color.b);
    textShader_->setUniform("opacity", opacity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cached.textureId);
    textShader_->setUniform("textTexture", 0);

    glBindVertexArray(vaoText_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ---------------------------------------------------------------------------
// computeDragDelta — unproject screen delta to world XYZ delta
// ---------------------------------------------------------------------------
void XYZPanGLView::computeDragDelta(const juce::Point<int>& screenDelta,
                                     float& outDX, float& outDY, float& outDZ)
{
    // Extract camera right and up from the view matrix column vectors.
    // View matrix [row][col] in GLM = column-major:
    //   col 0 = [viewMatrix_[0][0], viewMatrix_[1][0], viewMatrix_[2][0]] = camera right
    //   col 1 = [viewMatrix_[0][1], viewMatrix_[1][1], viewMatrix_[2][1]] = camera up
    const glm::vec3 camRight(viewMatrix_[0][0], viewMatrix_[1][0], viewMatrix_[2][0]);
    const glm::vec3 camUp   (viewMatrix_[0][1], viewMatrix_[1][1], viewMatrix_[2][1]);

    // Scale factor: map screen pixels to approximate world units
    const float scaleFactor = camera_.dist * 2.0f / static_cast<float>(getHeight());

    const float dx = static_cast<float>(screenDelta.x) * scaleFactor;
    const float dy = static_cast<float>(screenDelta.y) * scaleFactor;

    // World delta = dx * camRight - dy * camUp (screen y is flipped relative to world)
    const glm::vec3 worldDelta = camRight * dx - camUp * dy;

    // Map from GL convention to XYZPan convention:
    //   GL X  → XYZPan X  (left/right)
    //   GL Y  → XYZPan Z  (up/down)
    //   GL -Z → XYZPan Y  (front/back)
    outDX =  worldDelta.x;
    outDY = -worldDelta.z;   // GL -Z → XYZPan Y
    outDZ =  worldDelta.y;   // GL Y → XYZPan Z
}

// ---------------------------------------------------------------------------
// Nose draw methods
// ---------------------------------------------------------------------------

// Cone — the original forward-direction indicator, cone pointing in -Z
void XYZPanGLView::drawNoseCone(const glm::mat4& headRot, const AvatarParams& avatar,
                                 const glm::vec3& noseCol, float hs, float alpha)
{
    constexpr float kBaseRadius = 0.012f;
    constexpr float kLength     = 0.05f;
    constexpr float kOffset     = 0.048f;
    const float ns = avatar.noseSize;

    glm::mat4 m = headRot;
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, -kOffset * hs));
    m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::scale(m, glm::vec3(kBaseRadius * hs * ns, kLength * hs * ns, kBaseRadius * hs * ns));
    drawCone(m, noseCol, 0.9f * alpha);
}

// Button — small rounded sphere sitting on the face
void XYZPanGLView::drawNoseButton(const glm::mat4& headRot, const AvatarParams& avatar,
                                   const glm::vec3& noseCol, float hs, float alpha)
{
    constexpr float kRadius = 0.01f;
    constexpr float kOffset = 0.044f;
    const float ns = avatar.noseSize;

    glm::mat4 m = headRot;
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, -kOffset * hs));
    m = glm::scale(m, glm::vec3(kRadius * hs * ns));
    drawSphereWithModel(m, noseCol, 0.9f * alpha);
}

// Snout — elongated ellipsoid protruding forward
void XYZPanGLView::drawNoseSnout(const glm::mat4& headRot, const AvatarParams& avatar,
                                  const glm::vec3& noseCol, float hs, float alpha)
{
    constexpr float kRadiusXY = 0.014f;
    constexpr float kRadiusZ  = 0.03f;
    constexpr float kOffset   = 0.042f;
    const float ns = avatar.noseSize;

    glm::mat4 m = headRot;
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, -kOffset * hs));
    m = glm::scale(m, glm::vec3(kRadiusXY * hs * ns, kRadiusXY * hs * ns, kRadiusZ * hs * ns));
    drawSphereWithModel(m, noseCol, 0.9f * alpha);
}

// Clown — big red sphere (color defaults to theme nose but looks best overridden red)
void XYZPanGLView::drawNoseClown(const glm::mat4& headRot, const AvatarParams& avatar,
                                  const glm::vec3& noseCol, float hs, float alpha)
{
    constexpr float kRadius = 0.02f;
    constexpr float kOffset = 0.04f;
    const float ns = avatar.noseSize;

    glm::mat4 m = headRot;
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, -kOffset * hs));
    m = glm::scale(m, glm::vec3(kRadius * hs * ns));
    drawSphereWithModel(m, noseCol, 0.95f * alpha);
}

// Pointed — thin long cone, more exaggerated than Cone
void XYZPanGLView::drawNosePointed(const glm::mat4& headRot, const AvatarParams& avatar,
                                    const glm::vec3& noseCol, float hs, float alpha)
{
    constexpr float kBaseRadius = 0.008f;
    constexpr float kLength     = 0.07f;
    constexpr float kOffset     = 0.046f;
    const float ns = avatar.noseSize;

    glm::mat4 m = headRot;
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, -kOffset * hs));
    m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::scale(m, glm::vec3(kBaseRadius * hs * ns, kLength * hs * ns, kBaseRadius * hs * ns));
    drawCone(m, noseCol, 0.9f * alpha);
}

// ---------------------------------------------------------------------------
// Ear draw methods
// ---------------------------------------------------------------------------
void XYZPanGLView::drawEarsDefault(const glm::mat4& headRot, const AvatarParams& avatar,
                                    const glm::vec3& earCol, float hs, float alpha)
{
    constexpr float kEarRadius  = 0.015f;
    constexpr float kEarFlatten = 0.5f;
    constexpr float kEarOffset  = 0.045f;

    const float er = kEarRadius * avatar.earSize * hs;
    const float eo = kEarOffset * avatar.earOffset * hs;

    for (float side : {-1.0f, 1.0f}) {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(side * eo, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(side * avatar.earRotation), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, glm::vec3(er * kEarFlatten, er, er));
        drawSphereWithModel(m, earCol, 0.9f * alpha);
    }
}

void XYZPanGLView::drawEarsPointy(const glm::mat4& headRot, const AvatarParams& avatar,
                                   const glm::vec3& earCol, float hs, float alpha)
{
    constexpr float kEarRadius = 0.012f;
    constexpr float kEarLength = 0.035f;
    constexpr float kEarOffset = 0.045f;
    constexpr float kTiltAngle = glm::radians(30.0f);

    const float es = avatar.earSize * hs;
    const float eo = kEarOffset * avatar.earOffset * hs;

    for (float side : {-1.0f, 1.0f}) {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(side * eo, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(side * avatar.earRotation), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::rotate(m, side * kTiltAngle, glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, glm::vec3(kEarRadius * es, kEarLength * es, kEarRadius * es));
        drawCone(m, earCol, 0.9f * alpha);
    }
}

void XYZPanGLView::drawEarsRound(const glm::mat4& headRot, const AvatarParams& avatar,
                                  const glm::vec3& earCol, float hs, float alpha)
{
    constexpr float kEarRadius = 0.015f;
    constexpr float kEarOffset = 0.045f;

    const float er = kEarRadius * avatar.earSize * hs;
    const float eo = kEarOffset * avatar.earOffset * hs;

    for (float side : {-1.0f, 1.0f}) {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(side * eo, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(side * avatar.earRotation), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, glm::vec3(er, er, er));
        drawSphereWithModel(m, earCol, 0.9f * alpha);
    }
}

void XYZPanGLView::drawEarsCat(const glm::mat4& headRot, const AvatarParams& avatar,
                                const glm::vec3& earCol, float hs, float alpha)
{
    constexpr float kEarRadius  = 0.010f;
    constexpr float kEarLength  = 0.030f;
    constexpr float kEarSpacing = 0.022f;
    constexpr float kEarHeightY = 0.038f;
    constexpr float kTiltAngle  = glm::radians(20.0f);

    const float es = avatar.earSize * hs;
    const float headY = kEarHeightY * avatar.headElongation * hs;

    for (float side : {-1.0f, 1.0f}) {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(side * kEarSpacing * avatar.earOffset * hs,
                                         headY * 0.85f, 0.0f));
        m = glm::rotate(m, glm::radians(side * avatar.earRotation), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::rotate(m, side * kTiltAngle, glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, glm::vec3(kEarRadius * es, kEarLength * es, kEarRadius * es));
        drawCone(m, earCol, 0.9f * alpha);
    }
}

// ---------------------------------------------------------------------------
// Hat draw methods
// ---------------------------------------------------------------------------
void XYZPanGLView::drawHatParty(const glm::mat4& headRot, const AvatarParams& avatar,
                                 const glm::vec3& hatCol, float hs, float alpha)
{
    const float topY = 0.045f * avatar.headElongation * hs;
    const float sz = avatar.hatSize;
    constexpr float kConeRadius = 0.020f;
    constexpr float kConeHeight = 0.055f;

    glm::mat4 m = headRot;
    m = glm::translate(m, glm::vec3(0.0f, topY, 0.0f));
    m = glm::scale(m, glm::vec3(kConeRadius * hs * sz, kConeHeight * hs * sz, kConeRadius * hs * sz));
    drawCone(m, hatCol, 0.9f * alpha);
}

void XYZPanGLView::drawHatTopHat(const glm::mat4& headRot, const AvatarParams& avatar,
                                   const glm::vec3& hatCol, float hs, float alpha)
{
    const float topY = 0.045f * avatar.headElongation * hs;
    const float sz = avatar.hatSize;

    // Brim — flat sphere
    {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(0.0f, topY, 0.0f));
        m = glm::scale(m, glm::vec3(0.042f * hs * sz, 0.12f * 0.042f * hs * sz, 0.042f * hs * sz));
        drawSphereWithModel(m, hatCol, 0.9f * alpha);
    }
    // Body — tall sphere above brim
    {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(0.0f, topY + 0.035f * hs * sz, 0.0f));
        m = glm::scale(m, glm::vec3(0.026f * hs * sz, 0.035f * hs * sz, 0.026f * hs * sz));
        drawSphereWithModel(m, hatCol, 0.9f * alpha);
    }
}

void XYZPanGLView::drawHatHalo(const glm::mat4& headRot, const AvatarParams& avatar,
                                 const glm::vec3& hatCol, float hs, float alpha)
{
    const float topY = 0.045f * avatar.headElongation * hs;
    const float sz = avatar.hatSize;
    constexpr float kRingRadius  = 0.032f;
    constexpr float kSphereRadius = 0.005f;
    constexpr float kHoverHeight  = 0.020f;
    constexpr int   kCount = 12;

    for (int i = 0; i < kCount; ++i) {
        const float angle = static_cast<float>(i) * (2.0f * glm::pi<float>() / static_cast<float>(kCount));
        const float px = kRingRadius * hs * sz * std::cos(angle);
        const float pz = kRingRadius * hs * sz * std::sin(angle);

        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(px, topY + kHoverHeight * hs * sz, pz));
        m = glm::scale(m, glm::vec3(kSphereRadius * hs * sz));
        drawSphereWithModel(m, hatCol, 0.9f * alpha);
    }
}

void XYZPanGLView::drawHatBeanie(const glm::mat4& headRot, const AvatarParams& avatar,
                                   const glm::vec3& hatCol, float hs, float alpha)
{
    const float topY = 0.045f * avatar.headElongation * hs;
    const float sz = avatar.hatSize;

    glm::mat4 m = headRot;
    m = glm::translate(m, glm::vec3(0.0f, topY, 0.0f));
    m = glm::scale(m, glm::vec3(0.048f * hs * sz, 0.018f * hs * sz, 0.048f * hs * sz));
    drawSphereWithModel(m, hatCol, 0.9f * alpha);
}

void XYZPanGLView::drawHatDevilHorns(const glm::mat4& headRot, const AvatarParams& avatar,
                                        const glm::vec3& hatCol, float hs, float alpha)
{
    // Shift horns down slightly so they sit on the head rather than floating above
    const float topY = (0.045f - 0.006f) * avatar.headElongation * hs;
    const float sz = avatar.hatSize;
    constexpr float kHornRadius = 0.008f;
    constexpr float kHornHeight = 0.040f;
    constexpr float kHornSpread = 0.025f;
    constexpr float kHornTilt   = 0.30f; // radians outward tilt

    for (float side : {-1.0f, 1.0f}) {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(side * kHornSpread * hs * sz, topY, 0.0f));
        // Tilt horns outward
        m = glm::rotate(m, side * kHornTilt, glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, glm::vec3(kHornRadius * hs * sz, kHornHeight * hs * sz, kHornRadius * hs * sz));
        drawCone(m, hatCol, 0.9f * alpha);
    }
}


void XYZPanGLView::drawHatPonytail(const glm::mat4& headRot, const AvatarParams& avatar,
                                      const glm::vec3& hatCol, float hs, float alpha)
{
    const float topY = 0.045f * avatar.headElongation * hs;
    const float sz = avatar.hatSize;
    // Hair tie — small torus-like ring of spheres at the back-top of head
    constexpr float kTieY      = -0.005f;
    constexpr float kTieZ      =  0.035f;  // behind head (+Z = back)
    constexpr float kTieRadius = 0.010f;
    constexpr float kTieBead   = 0.004f;
    constexpr int   kTieCount  = 6;

    for (int i = 0; i < kTieCount; ++i) {
        const float angle = static_cast<float>(i) * (2.0f * glm::pi<float>() / static_cast<float>(kTieCount));
        const float px = kTieRadius * hs * sz * std::cos(angle);
        const float py = kTieRadius * hs * sz * std::sin(angle);

        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(px, topY + kTieY * hs * sz + py, kTieZ * hs * sz));
        m = glm::scale(m, glm::vec3(kTieBead * hs * sz));
        drawSphereWithModel(m, hatCol, 0.9f * alpha);
    }

    // Tail segments — chain of spheres drooping downward behind the head
    constexpr int   kSegments   = 5;
    constexpr float kSegRadius  = 0.009f;
    constexpr float kDropPer    = 0.018f;  // vertical drop per segment
    constexpr float kBackPer    = 0.005f;  // drift further back per segment

    for (int i = 0; i < kSegments; ++i) {
        const float fi = static_cast<float>(i);
        // Taper radius along the tail
        const float r = kSegRadius * (1.0f - fi * 0.12f);

        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(0.0f,
                                         topY + kTieY * hs * sz - kDropPer * (fi + 1.0f) * hs * sz,
                                         (kTieZ + kBackPer * (fi + 1.0f)) * hs * sz));
        m = glm::scale(m, glm::vec3(r * hs * sz, r * 0.8f * hs * sz, r * hs * sz));
        drawSphereWithModel(m, hatCol, 0.85f * alpha);
    }
}

// ---------------------------------------------------------------------------
// Eye draw methods
// ---------------------------------------------------------------------------
void XYZPanGLView::drawEyesNormal(const glm::mat4& headRot, const AvatarParams& avatar,
                                   float hs, float headAlpha)
{
    constexpr float kBaseEyeRadius  = 0.008f;
    constexpr float kBasePupilRatio = 0.5f;
    constexpr float kBaseEyeSpacing = 0.018f;
    constexpr float kBaseEyeForward = 0.038f;
    constexpr float kBaseEyeHeight  = 0.012f;

    const float eyeR    = kBaseEyeRadius * avatar.eyeSize * hs;
    const float pupilRatio = kBasePupilRatio + avatar.pupilSize * (1.0f - kBasePupilRatio);
    const float pupilR  = eyeR * pupilRatio;
    const float spacing = kBaseEyeSpacing * avatar.eyeSpacing * hs;
    const float eyeY    = kBaseEyeHeight * avatar.eyeHeight * hs * avatar.headElongation;
    const float eyeZ    = kBaseEyeForward * hs;

    // Pupil moves toward eye center as it grows: max 60% of the way at full size
    const float pupilForward = eyeR * 0.6f * (1.0f - avatar.pupilSize * 0.6f);

    const glm::vec3 eyeWhite = avatar.eyeColor ? toVec3(avatar.eyeColor)
                                                : glm::vec3(0.92f, 0.90f, 0.85f);
    const glm::vec3 pupilColor(0.08f, 0.06f, 0.04f);

    for (float side : {-1.0f, 1.0f}) {
        {
            glm::mat4 m = headRot;
            m = glm::translate(m, glm::vec3(side * spacing, eyeY, -eyeZ));
            m = glm::scale(m, glm::vec3(eyeR));
            drawSphereWithModel(m, eyeWhite, 0.95f * headAlpha);
        }
        if (avatar.pupilSize > 0.0f) {
            glm::mat4 m = headRot;
            m = glm::translate(m, glm::vec3(side * spacing, eyeY, -eyeZ));
            m = glm::translate(m, glm::vec3(0.0f, 0.0f, -pupilForward));
            m = glm::scale(m, glm::vec3(pupilR));
            drawSphereWithModel(m, pupilColor, 0.95f * headAlpha);
        }
    }
}

void XYZPanGLView::drawEyesGoogly(const glm::mat4& headRot, const AvatarParams& avatar,
                                    float hs, float headAlpha)
{
    constexpr float kBaseEyeRadius  = 0.008f;
    constexpr float kGooglyScale    = 1.4f;
    constexpr float kBasePupilRatio = 0.45f;
    constexpr float kBaseEyeSpacing = 0.018f;
    constexpr float kBaseEyeForward = 0.038f;
    constexpr float kBaseEyeHeight  = 0.012f;

    const float eyeR    = kBaseEyeRadius * avatar.eyeSize * hs * kGooglyScale;
    const float pupilRatio = kBasePupilRatio + avatar.pupilSize * (1.0f - kBasePupilRatio);
    const float pupilR  = eyeR * pupilRatio;
    const float spacing = kBaseEyeSpacing * avatar.eyeSpacing * hs;
    const float eyeY    = kBaseEyeHeight * avatar.eyeHeight * hs * avatar.headElongation;
    const float eyeZ    = kBaseEyeForward * hs;

    const glm::vec3 eyeWhite = avatar.eyeColor ? toVec3(avatar.eyeColor)
                                                : glm::vec3(0.95f, 0.93f, 0.88f);
    const glm::vec3 pupilColor(0.05f, 0.04f, 0.03f);

    GooglyEyeState* states[2] = { &googlyLeft_, &googlyRight_ };
    float sides[2] = { -1.0f, 1.0f };

    for (int i = 0; i < 2; ++i) {
        const float side = sides[i];
        const auto& state = *states[i];

        // Eyeball
        {
            glm::mat4 m = headRot;
            m = glm::translate(m, glm::vec3(side * spacing, eyeY, -eyeZ));
            m = glm::scale(m, glm::vec3(eyeR));
            drawSphereWithModel(m, eyeWhite, 0.95f * headAlpha);
        }
        // Pupil — offset by physics state, projected onto eyeball sphere surface
        if (avatar.pupilSize > 0.0f) {
            const float pox = state.pupilOffset.x * eyeR;
            const float poy = state.pupilOffset.y * eyeR;
            // Project onto sphere: z = sqrt(1 - x^2 - y^2) * eyeR
            const float r2 = state.pupilOffset.x * state.pupilOffset.x
                           + state.pupilOffset.y * state.pupilOffset.y;
            const float zProj = std::sqrt(std::max(0.0f, 1.0f - r2)) * eyeR;

            // Pupil moves toward eye center as it grows: max 60% of the way at full size
            const float centerBlend = avatar.pupilSize * 0.6f;

            glm::mat4 m = headRot;
            m = glm::translate(m, glm::vec3(side * spacing, eyeY, -eyeZ));
            m = glm::translate(m, glm::vec3(pox * (1.0f - centerBlend),
                                             poy * (1.0f - centerBlend),
                                             -zProj * 0.6f * (1.0f - centerBlend)));
            m = glm::scale(m, glm::vec3(pupilR));
            drawSphereWithModel(m, pupilColor, 0.95f * headAlpha);
        }
    }
}

void XYZPanGLView::drawEyesXEyes(const glm::mat4& headRot, const AvatarParams& avatar,
                                   float hs, float headAlpha)
{
    constexpr float kBaseEyeSpacing = 0.018f;
    constexpr float kBaseEyeForward = 0.038f;
    constexpr float kBaseEyeHeight  = 0.012f;
    constexpr float kCrossRadius    = 0.006f;
    constexpr float kCrossLength    = 0.012f;

    const float spacing = kBaseEyeSpacing * avatar.eyeSpacing * hs;
    const float eyeY    = kBaseEyeHeight * avatar.eyeHeight * hs * avatar.headElongation;
    const float eyeZ    = kBaseEyeForward * hs;
    const float cr      = kCrossRadius * avatar.eyeSize * hs;
    const float cl      = kCrossLength * avatar.eyeSize * hs;

    const glm::vec3 xColor(0.08f, 0.06f, 0.04f);

    for (float side : {-1.0f, 1.0f}) {
        glm::vec3 center(side * spacing, eyeY, -eyeZ);

        // Base transform: translate to eye center
        glm::mat4 base = headRot;
        base = glm::translate(base, center);

        // Two crossed cones per eye forming an X shape
        // Diagonal 1: lower-left to upper-right (45°)
        {
            glm::mat4 m = base;
            m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::scale(m, glm::vec3(cr * 0.4f, cl, cr * 0.4f));
            drawCone(m, xColor, 0.9f * headAlpha);
        }
        // Diagonal 1 reverse
        {
            glm::mat4 m = base;
            m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            m = glm::scale(m, glm::vec3(cr * 0.4f, cl, cr * 0.4f));
            drawCone(m, xColor, 0.9f * headAlpha);
        }
        // Diagonal 2: upper-left to lower-right (-45°)
        {
            glm::mat4 m = base;
            m = glm::rotate(m, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::scale(m, glm::vec3(cr * 0.4f, cl, cr * 0.4f));
            drawCone(m, xColor, 0.9f * headAlpha);
        }
        // Diagonal 2 reverse
        {
            glm::mat4 m = base;
            m = glm::rotate(m, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            m = glm::scale(m, glm::vec3(cr * 0.4f, cl, cr * 0.4f));
            drawCone(m, xColor, 0.9f * headAlpha);
        }
    }
}

void XYZPanGLView::drawEyesCyclops(const glm::mat4& headRot, const AvatarParams& avatar,
                                     float hs, float headAlpha)
{
    constexpr float kBaseEyeRadius  = 0.008f;
    constexpr float kCyclopsScale   = 1.5f;
    constexpr float kBasePupilRatio = 0.5f;
    constexpr float kBaseEyeForward = 0.038f;
    constexpr float kBaseEyeHeight  = 0.012f;

    const float eyeR   = kBaseEyeRadius * avatar.eyeSize * hs * kCyclopsScale;
    const float pupilRatio = kBasePupilRatio + avatar.pupilSize * (1.0f - kBasePupilRatio);
    const float pupilR = eyeR * pupilRatio;
    const float eyeY   = kBaseEyeHeight * avatar.eyeHeight * hs * avatar.headElongation;
    const float eyeZ   = kBaseEyeForward * hs;

    // Pupil moves toward eye center as it grows: max 60% of the way at full size
    const float pupilForward = eyeR * 0.6f * (1.0f - avatar.pupilSize * 0.6f);

    const glm::vec3 eyeWhite = avatar.eyeColor ? toVec3(avatar.eyeColor)
                                                : glm::vec3(0.92f, 0.90f, 0.85f);
    const glm::vec3 pupilColor(0.08f, 0.06f, 0.04f);

    // Single centered eye (ignores eyeSpacing)
    {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(0.0f, eyeY, -eyeZ));
        m = glm::scale(m, glm::vec3(eyeR));
        drawSphereWithModel(m, eyeWhite, 0.95f * headAlpha);
    }
    if (avatar.pupilSize > 0.0f) {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(0.0f, eyeY, -eyeZ));
        m = glm::translate(m, glm::vec3(0.0f, 0.0f, -pupilForward));
        m = glm::scale(m, glm::vec3(pupilR));
        drawSphereWithModel(m, pupilColor, 0.95f * headAlpha);
    }
}

// ---------------------------------------------------------------------------
// Googly eye spring-damper physics with source-object gravity
// ---------------------------------------------------------------------------
void XYZPanGLView::updateGooglyPhysics(const SourcePositionSnapshot& snap, double now,
                                         const glm::vec3& sourcePos,
                                         const glm::vec3& lNodePos,
                                         const glm::vec3& rNodePos,
                                         bool stereoActive,
                                         const AvatarParams& avatar)
{
    // First-frame sentinel: initialize prev values, skip impulse
    if (prevFrameTime_ < 0.0) {
        prevListenerYaw_   = snap.listenerYaw;
        prevListenerPitch_ = snap.listenerPitch;
        prevFrameTime_     = now;
        return;
    }

    float dt = static_cast<float>(now - prevFrameTime_);
    prevFrameTime_ = now;

    // Clamp dt to avoid explosion after pause/stall
    dt = std::min(dt, 0.05f);
    if (dt <= 0.0f) return;

    const float deltaYaw   = snap.listenerYaw   - prevListenerYaw_;
    const float deltaPitch = snap.listenerPitch  - prevListenerPitch_;
    prevListenerYaw_   = snap.listenerYaw;
    prevListenerPitch_ = snap.listenerPitch;

    // Impulse: head rotates right → pupil goes left
    constexpr float kImpulseScale = 3.0f;
    const glm::vec2 impulse(-deltaYaw * kImpulseScale / dt,
                            -deltaPitch * kImpulseScale / dt);

    constexpr float kSpringMax    = 80.0f;
    constexpr float kDampingMax   = 12.0f;
    constexpr float kGravityForce = 120.0f;
    constexpr float kMaxRange     = 0.6f;
    constexpr float kMinRange     = 0.04f;

    // Eye positioning constants (must match drawEyesGoogly)
    constexpr float kBaseEyeSpacing = 0.018f;
    constexpr float kBaseEyeForward = 0.038f;
    constexpr float kBaseEyeHeight  = 0.012f;

    const float gravity = avatar.googlyGravity;
    const float springW = avatar.googlySpring;  // 0=free spin, 1=full return
    const float kSpring  = kSpringMax  * springW;
    const float kDamping = kDampingMax * springW;
    const float hs = avatar.headSize;

    // Reconstruct head rotation matrix (same as renderOpenGL headRot)
    glm::mat4 headRot(1.0f);
    headRot = glm::rotate(headRot, snap.listenerYaw,   glm::vec3(0.0f, 1.0f, 0.0f));
    headRot = glm::rotate(headRot, snap.listenerPitch,  glm::vec3(1.0f, 0.0f, 0.0f));
    headRot = glm::rotate(headRot, snap.listenerRoll,   glm::vec3(0.0f, 0.0f, -1.0f));
    const glm::mat3 headRot3(headRot);
    const glm::mat3 invHeadRot = glm::transpose(headRot3);

    const float spacing = kBaseEyeSpacing * avatar.eyeSpacing * hs;
    const float eyeY    = kBaseEyeHeight * avatar.eyeHeight * hs * avatar.headElongation;
    const float eyeZ    = kBaseEyeForward * hs;

    auto stepEye = [&](GooglyEyeState& eye, float eyeSideSign) {
        // Compute gravity acceleration from nearest source
        glm::vec2 gravityAccel(0.0f);
        if (gravity > 0.0f) {
            // Eye world position
            glm::vec3 eyeLocal(eyeSideSign * spacing, eyeY, -eyeZ);
            glm::vec3 eyeWorld = headRot3 * eyeLocal;

            // Pick nearest source
            glm::vec3 target = sourcePos;
            if (stereoActive) {
                float dL = glm::length(lNodePos - eyeWorld);
                float dR = glm::length(rNodePos - eyeWorld);
                target = (dL <= dR) ? lNodePos : rNodePos;
            }

            glm::vec3 toSource = target - eyeWorld;
            float dist = glm::length(toSource);

            float effectiveRange = kMinRange + gravity * (kMaxRange - kMinRange);
            if (dist < effectiveRange && dist > 1e-6f) {
                // Smoothstep falloff
                float t = 1.0f - dist / effectiveRange;
                float proximity = t * t * (3.0f - 2.0f * t);

                // Project 3D eye→source into head-local 2D (X=lateral, Y=vertical)
                glm::vec3 localDir = invHeadRot * toSource;
                glm::vec2 dir2D(localDir.x, localDir.y);
                float len2D = glm::length(dir2D);
                if (len2D > 1e-6f)
                    gravityAccel = (dir2D / len2D) * kGravityForce * gravity * proximity;
            }
        }

        // Semi-implicit Euler
        glm::vec2 accel = -kSpring * eye.pupilOffset - kDamping * eye.velocity
                        + impulse + gravityAccel;
        eye.velocity += accel * dt;
        eye.pupilOffset += eye.velocity * dt;

        // Hard clamp to unit circle
        float len2 = glm::dot(eye.pupilOffset, eye.pupilOffset);
        if (len2 > 1.0f) {
            float len = std::sqrt(len2);
            eye.pupilOffset /= len;
            // Zero outward velocity component
            float radialVel = glm::dot(eye.velocity, eye.pupilOffset);
            if (radialVel > 0.0f)
                eye.velocity -= radialVel * eye.pupilOffset;
        }
    };

    stepEye(googlyLeft_,  -1.0f);
    stepEye(googlyRight_,  1.0f);
}

} // namespace xyzpan

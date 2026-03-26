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
                             xyzpan::ForeignSourceBridge& foreignBridge)
    : apvts_(apvts), proc_(proc), bridge_(bridge), foreignBridge_(foreignBridge)
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
}

XYZPanGLView::~XYZPanGLView()
{
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

}

// ---------------------------------------------------------------------------
// parameterChanged — bidirectional head-follows: knob → camera
// ---------------------------------------------------------------------------
void XYZPanGLView::parameterChanged(const juce::String& id, float newValue)
{
    if (!headFollowsActive_ || drivingParamsFromCamera_->load(std::memory_order_relaxed))
        return;

    if (id == "listener_yaw")
        camera_.yaw = newValue;              // both use 0–360°
    else if (id == "listener_pitch")
        camera_.pitch = -newValue;           // camera pitch is negated relative to param
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

        if (headFollowsActive_ && toggleOn) {
            // Map camera yaw/pitch to listener params (wrap to 0–360°)
            // camera_.yaw and camera_.pitch are already in degrees
            float camYawDeg = std::fmod(camera_.yaw, 360.0f);
            if (camYawDeg < 0.0f) camYawDeg += 360.0f;
            float camPitchDeg = std::fmod(-camera_.pitch, 360.0f);
            if (camPitchDeg < 0.0f) camPitchDeg += 360.0f;

            // Guard: suppress parameterChanged echoes from our own writes.
            // shared_ptr captured by value so lambda is safe if GLView is destroyed first.
            auto flag = drivingParamsFromCamera_;
            auto* apvtsPtr = &apvts_;
            flag->store(true, std::memory_order_relaxed);
            juce::MessageManager::callAsync([flag, apvtsPtr, camYawDeg, camPitchDeg]() {
                if (auto* p = apvtsPtr->getParameter("listener_yaw"))
                    p->setValueNotifyingHost(p->convertTo0to1(camYawDeg));
                if (auto* p = apvtsPtr->getParameter("listener_pitch"))
                    p->setValueNotifyingHost(p->convertTo0to1(camPitchDeg));
                if (auto* p = apvtsPtr->getParameter("listener_roll"))
                    p->setValueNotifyingHost(p->convertTo0to1(0.0f));
                flag->store(false, std::memory_order_relaxed);
            });
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

    // Draw audible radius sphere — semi-transparent boundary at origin
    // Wireframe grid overlay stays visible from inside the sphere (GL_LINES have no facing).
    {
        glDepthMask(GL_FALSE);
        drawSphere(glm::vec3(0.0f), sr, theme.glAudibleSphere, 0.08f);
        drawSphereWireframe(sr, theme.glAudibleSphere, 0.04f);

        // Re-establish sphere shader batch for source/stereo node draws that follow
        sphereShader_->use();
        sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
        sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
        const glm::vec3 lightDir = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
        sphereShader_->setUniform("lightDir", lightDir.x, lightDir.y, lightDir.z);
        glBindVertexArray(vaoSphere_);
        glDepthMask(GL_TRUE);
    }

    // Draw source node at current position
    // 0.8x base size; 10% opacity when stereo split is active.
    // When stereo is active, skip depth write so the ghost center doesn't
    // occlude the L/R nodes orbiting behind it.
    {
        const glm::vec3 sourceColor = isSourceHovered_
            ? theme.glSourceHover
            : theme.glSourceNormal;
        const float mainOpacity = stereoActive ? 0.1f : sourceOpacity;
        const float mainRadius = stereoActive ? 0.0375f : 0.048f;
        if (stereoActive) glDepthMask(GL_FALSE);
        drawSphere(sourcePos, mainRadius, sourceColor, mainOpacity);
        if (stereoActive) glDepthMask(GL_TRUE);
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

    // Foreign (linked instance) source nodes
    {
        const auto fp = foreignBridge_.read();
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
        for (int i = 0; i < fp.count; ++i) {
            const auto& fs = fp.sources[i];
            const auto color = kPalette[fs.colorIndex % 8];
            const glm::vec3 fPos(fs.x, fs.z, -fs.y);  // XYZPan → GL
            drawSphere(fPos, 0.035f, color, 0.6f);
            if (fs.stereoWidth > 0.0f) {
                const glm::vec3 fL(fs.lNodeX, fs.lNodeZ, -fs.lNodeY);
                const glm::vec3 fR(fs.rNodeX, fs.rNodeZ, -fs.rNodeY);
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

}

// ---------------------------------------------------------------------------
// Mouse: Down
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseDown(const juce::MouseEvent& e)
{
    lastDragPos_ = e.getPosition();

    if (isNearSourceNode(e.getPosition())) {
        isDraggingSource_ = true;
        isDraggingCamera_ = false;
        setMouseCursor(juce::MouseCursor(juce::MouseCursor::DraggingHandCursor));
    } else {
        isDraggingSource_ = false;
        isDraggingCamera_ = true;
    }
}

// ---------------------------------------------------------------------------
// Mouse: Drag
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseDrag(const juce::MouseEvent& e)
{
    const juce::Point<int> currentPos = e.getPosition();
    const juce::Point<int> delta = currentPos - lastDragPos_;
    lastDragPos_ = currentPos;

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

        // NEVER write APVTS from GL/render thread — post to message thread
        // Capture APVTS by pointer (safe: proc lifetime > editor lifetime)
        juce::AudioProcessorValueTreeState* apvtsPtr = &apvts_;
        juce::MessageManager::callAsync(
            [apvtsPtr, newX, newY, newZ]() {
                if (auto* p = apvtsPtr->getParameter(kParamX))
                    p->setValueNotifyingHost(apvtsPtr->getParameterRange(kParamX).convertTo0to1(newX));
                if (auto* p = apvtsPtr->getParameter(kParamY))
                    p->setValueNotifyingHost(apvtsPtr->getParameterRange(kParamY).convertTo0to1(newY));
                if (auto* p = apvtsPtr->getParameter(kParamZ))
                    p->setValueNotifyingHost(apvtsPtr->getParameterRange(kParamZ).convertTo0to1(newZ));
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
    glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, nullptr);
}

// ---------------------------------------------------------------------------
// drawSphereWireframe — lat/long grid via GL_LINES using lineShader_
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSphereWireframe(float radius, const glm::vec3& color, float opacity)
{
    if (!lineShader_ || vaoSphereWire_ == 0 || sphereWireIndexCount_ == 0) return;

    const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(radius));

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

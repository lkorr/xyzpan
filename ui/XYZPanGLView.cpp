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

    // Cache param pointers for gesture management (automation recording)
    cachedYawParam_   = apvts_.getParameter("listener_yaw");
    cachedPitchParam_ = apvts_.getParameter("listener_pitch");
    cachedRollParam_  = apvts_.getParameter("listener_roll");

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

    // End any in-flight automation gesture before tearing down
    if (headFollowsGestureActive_) {
        if (cachedYawParam_)   cachedYawParam_->endChangeGesture();
        if (cachedPitchParam_) cachedPitchParam_->endChangeGesture();
        if (cachedRollParam_)  cachedRollParam_->endChangeGesture();
        headFollowsGestureActive_ = false;
    }

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

    // Build source shape meshes (pyramid, cube, octahedron, torus)
    uploadSourceShapeVAOs();

    // Create trail VAO/VBOs
    createTrailVAO(vaoTrailSource_, vboTrailSource_, TrailBuffer::kCapacity);
    createTrailVAO(vaoTrailL_,      vboTrailL_,      TrailBuffer::kCapacity);
    createTrailVAO(vaoTrailR_,      vboTrailR_,      TrailBuffer::kCapacity);
    createTrailVAO(vaoTrailListener_, vboTrailListener_, TrailBuffer::kCapacity);

    // Text billboard quad
    uploadTextQuadVAO();

    // Skybox fullscreen quad (clip-space [-1,1])
    {
        const float skyQuad[] = { -1.f,-1.f,  1.f,-1.f,  -1.f,1.f,  1.f,1.f };
        glGenVertexArrays(1, &vaoSkybox_);
        glGenBuffers(1, &vboSkybox_);
        glBindVertexArray(vaoSkybox_);
        glBindBuffer(GL_ARRAY_BUFFER, vboSkybox_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyQuad), skyQuad, GL_STATIC_DRAW);
        if (skyboxShader_) {
            auto posAttr = glGetAttribLocation(skyboxShader_->getProgramID(), "position");
            if (posAttr >= 0) {
                glEnableVertexAttribArray(static_cast<GLuint>(posAttr));
                glVertexAttribPointer(static_cast<GLuint>(posAttr), 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            }
        }
        glBindVertexArray(0);
    }

    // Ground plane — subdivided grid for hills displacement
    {
        const float groundY = -4.5f;
        const float halfExt = 150.0f;
        const int gridN = 128;  // 128x128 grid = 16384 vertices
        const int vertCount = (gridN + 1) * (gridN + 1);
        std::vector<float> verts(vertCount * 3);
        for (int iz = 0; iz <= gridN; ++iz) {
            for (int ix = 0; ix <= gridN; ++ix) {
                const int vi = (iz * (gridN + 1) + ix) * 3;
                verts[vi + 0] = -halfExt + (2.0f * halfExt * ix) / gridN;
                verts[vi + 1] = groundY;
                verts[vi + 2] = -halfExt + (2.0f * halfExt * iz) / gridN;
            }
        }
        // Triangle indices (two triangles per cell)
        const int cellCount = gridN * gridN;
        std::vector<unsigned> indices(cellCount * 6);
        int idx = 0;
        for (int iz = 0; iz < gridN; ++iz) {
            for (int ix = 0; ix < gridN; ++ix) {
                unsigned tl = static_cast<unsigned>(iz * (gridN + 1) + ix);
                unsigned tr = tl + 1;
                unsigned bl = tl + static_cast<unsigned>(gridN + 1);
                unsigned br = bl + 1;
                indices[idx++] = tl; indices[idx++] = bl; indices[idx++] = tr;
                indices[idx++] = tr; indices[idx++] = bl; indices[idx++] = br;
            }
        }
        groundIndexCount_ = static_cast<int>(indices.size());

        glGenVertexArrays(1, &vaoGround_);
        glGenBuffers(1, &vboGround_);
        glGenBuffers(1, &iboGround_);
        glBindVertexArray(vaoGround_);
        glBindBuffer(GL_ARRAY_BUFFER, vboGround_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                     verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboGround_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)),
                     indices.data(), GL_STATIC_DRAW);
        if (groundShader_) {
            auto posAttr = glGetAttribLocation(groundShader_->getProgramID(), "position");
            if (posAttr >= 0) {
                glEnableVertexAttribArray(static_cast<GLuint>(posAttr));
                glVertexAttribPointer(static_cast<GLuint>(posAttr), 3, GL_FLOAT, GL_FALSE, 0, nullptr);
            }
        }
        glBindVertexArray(0);
    }
}

// ---------------------------------------------------------------------------
// parameterChanged — bidirectional head-follows: knob → camera
// ---------------------------------------------------------------------------
void XYZPanGLView::parameterChanged(const juce::String& id, float newValue)
{
    if (!headFollowsActive_
        || (accumulator_ && accumulator_->drivingFromInput->load(std::memory_order_relaxed))
        || (receivingBroadcast_ && receivingBroadcast_->load(std::memory_order_relaxed))
        || linkedNonPilot_.load(std::memory_order_relaxed) != 0)
        return;

    // Path B: external write (DAW automation / knob) -> sync accumulator + camera
    if (accumulator_) {
        float y = apvts_.getRawParameterValue("listener_yaw")->load();
        float p = apvts_.getRawParameterValue("listener_pitch")->load();
        float r = apvts_.getRawParameterValue("listener_roll")->load();
        accumulator_->syncFromRPY(y, p, r);
    }

    if (id == "listener_yaw")
        camera_.yaw = newValue;
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

void XYZPanGLView::setSceneParams(const SceneParams& params)
{
    const juce::SpinLock::ScopedLockType lock(customizeLock_);
    sceneParams_ = params;
}

// ---------------------------------------------------------------------------
// renderOpenGL — called every frame on the GL thread
// ---------------------------------------------------------------------------
void XYZPanGLView::renderOpenGL()
{
    jassert(juce::OpenGLHelpers::isContextActive());

    // Snapshot theme + avatar + scene under lock (fast — POD copies)
    ColorTheme   theme;
    AvatarParams avatar;
    SceneParams  scene;
    {
        const juce::SpinLock::ScopedLockType lock(customizeLock_);
        theme  = glTheme_;
        avatar = avatarParams_;
        scene  = sceneParams_;
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

    // Distance-based opacity: full up to 50% of radius, gentle fade to 80%
    // at the sphere boundary, then rapid exponential falloff outside.
    auto distanceOpacity = [](float df) -> float {
        if (df <= 0.5f) return 1.0f;
        if (df <= 1.0f) return 1.0f - 0.4f * (df - 0.5f);   // 1.0 → 0.8
        return std::max(0.8f * std::exp(-4.0f * (df - 1.0f)), 0.1f);
    };
    const float distFrac = snap.distance / std::max(sr, 0.001f);
    const float sourceOpacity = distanceOpacity(distFrac);

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

    // --- Skybox (drawn behind everything) ---
    if (scene.skyType != kSkyNone && skyboxShader_ && vaoSkybox_ != 0) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        skyboxShader_->use();
        const glm::mat4 invVP = glm::inverse(projMatrix_ * viewMatrix_);
        skyboxShader_->setUniformMat4("invViewProj", glm::value_ptr(invVP), 1, GL_FALSE);
        skyboxShader_->setUniform("skyType", scene.skyType);
        skyboxShader_->setUniform("iTime", static_cast<float>(std::fmod(now, 1000.0)));
        glBindVertexArray(vaoSkybox_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    // --- Ground plane (drawn before scene geometry, with polygon offset to avoid z-fighting with grid) ---
    if (scene.groundType != kGroundNone && groundShader_ && vaoGround_ != 0) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        groundShader_->use();
        groundShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
        groundShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
        groundShader_->setUniform("groundType", scene.groundType);
        groundShader_->setUniform("iTime", static_cast<float>(std::fmod(now, 1000.0)));
        groundShader_->setUniform("fogColor", theme.glBackground.x, theme.glBackground.y, theme.glBackground.z);
        groundShader_->setUniform("groundYOffset", -4.5f * scene.groundHeight);
        groundShader_->setUniform("groundHills", scene.groundHills);
        glBindVertexArray(vaoGround_);
        glDrawElements(GL_TRIANGLES, groundIndexCount_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

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
    // IMPORTANT: only the pilot (or unlinked instance) may drive head-follows.
    // Non-pilot instances receive orientation via broadcast — activating
    // head-follows on them would lock their camera and fight the broadcast.
    {
        const bool isNonPilot = linkedNonPilot_.load(std::memory_order_relaxed) != 0;
        const bool toggleOn = !isNonPilot
                           && apvts_.getRawParameterValue("head_follows_camera")->load() >= 0.5f;

        if (toggleOn && !headFollowsActive_) {
            // Entering head-follows mode: save current param values and seed accumulator
            savedYawDeg_   = apvts_.getRawParameterValue("listener_yaw")->load();
            savedPitchDeg_ = apvts_.getRawParameterValue("listener_pitch")->load();
            savedRollDeg_  = apvts_.getRawParameterValue("listener_roll")->load();
            if (accumulator_)
                accumulator_->syncFromRPY(savedYawDeg_, savedPitchDeg_, savedRollDeg_);
            headFollowsActive_ = true;
        }

        if (headFollowsActive_ && toggleOn) {
            // Camera follows head: read listener angles (radians) and apply to camera.
            camera_.yaw   = glm::degrees(snap.listenerYaw);
            camera_.pitch = glm::degrees(snap.listenerPitch);
            camera_.roll  = glm::degrees(snap.listenerRoll);
            camera_.syncQuatFromEuler();
        }

        if (headFollowsActive_ && !toggleOn) {
            // Exiting head-follows mode: retain current camera-driven angles
            headFollowsActive_ = false;
            // End any in-flight automation gesture
            if (headFollowsGestureActive_) {
                headFollowsGestureActive_ = false;
                auto* pYaw   = cachedYawParam_;
                auto* pPitch = cachedPitchParam_;
                auto* pRoll  = cachedRollParam_;
                auto alive = glAlive_;
                juce::MessageManager::callAsync([pYaw, pPitch, pRoll, alive]() {
                    if (!alive->load(std::memory_order_acquire)) return;
                    if (pYaw)   pYaw->endChangeGesture();
                    if (pPitch) pPitch->endChangeGesture();
                    if (pRoll)  pRoll->endChangeGesture();
                });
            }
        }
    }

    // Draw listener node at origin — themed color, avatar-parameterized geometry
    if (headAlpha > 0.0f) {
        // Transparent body types: write depth via a color-masked pre-pass so the
        // observer head correctly occludes objects behind it, then draw the visible
        // transparent geometry on top without depth writes.
        const bool transparentBody = headAlpha < 1.0f
                                   || avatar.bodyType == kBodyGrid
                                   || avatar.bodyType == kBodyGhost
                                   || avatar.bodyType == kBodyGlass
                                   || avatar.bodyType == kBodyNone;

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
        // Y-scaled by headElongation, uniformly scaled by headSize.
        // Body type selects rendering style via drawAvatarSphere.
        {
            constexpr float kBaseHeadRadius = 0.045f;
            glm::mat4 headModel = headRot;
            headModel = glm::scale(headModel, glm::vec3(kBaseHeadRadius * hs,
                                                         kBaseHeadRadius * hs * avatar.headElongation,
                                                         kBaseHeadRadius * hs));

            // Depth pre-pass for transparent bodies: write depth only (no color)
            // so the head silhouette occludes objects behind it.
            if (transparentBody && avatar.bodyType != kBodyNone) {
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glDepthMask(GL_TRUE);
                drawSphereWithModel(headModel, headCol, 1.0f);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            }

            if (transparentBody)
                glDepthMask(GL_FALSE);

            drawAvatarSphere(headModel, headCol, headAlpha, avatar.bodyType);
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
        if (scene.showArrow) {
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

        if (transparentBody)
            glDepthMask(GL_TRUE);
    }

    // Audible sphere visibility: always-on toggle OR sphere knob hover/drag
    const bool alwaysShowSphere = showAudibleSphereParam_ == nullptr
                                  || showAudibleSphereParam_->load(std::memory_order_relaxed) >= 0.5f;
    const bool sphereKnobActive = sphereKnobActive_.load(std::memory_order_relaxed) != 0;
    const bool showSphere = alwaysShowSphere || sphereKnobActive;

    // Draw audible radius sphere + wireframe centered on the source node
    {
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
        // When stereo is active, emit from L/R nodes individually; otherwise from center.
        if (stereoActive) {
            // Reset L/R emitters on mono→stereo transition
            if (!prevStereoActive_) {
                ownWaveEmitterL_ = WaveEmitter{};
                ownWaveEmitterR_ = WaveEmitter{};
            }
            drawSoundWaves(ownWaveEmitterL_, lNodePos, sr, smoothedRms_, theme.glStereoL, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);
            drawSoundWaves(ownWaveEmitterR_, rNodePos, sr, smoothedRms_, theme.glStereoR, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);
        } else {
            // Reset center emitter on stereo→mono transition
            if (prevStereoActive_)
                ownWaveEmitter_ = WaveEmitter{};
            drawSoundWaves(ownWaveEmitter_, sourcePos, sr, smoothedRms_, theme.glAudibleSphere, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);
        }
        prevStereoActive_ = stereoActive;

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
            drawSourceShape(sourcePos, mainRadius, sourceColor, mainOpacity, scene.sourceShape, now, scene.clusterCount);
            if (stereoActive) glDepthMask(GL_TRUE);
        }
    }

    // Draw L/R stereo node spheres (smaller than center, only when stereo active)
    // Each node's opacity is based on its distance to the listener.
    // Draw back-to-front (farther node first) so the nearer semi-transparent
    // node blends correctly on top of the farther one.
    if (stereoActive) {
        const float lDistFrac = glm::length(lNodePos - listenerPos) / std::max(sr, 0.001f);
        const float rDistFrac = glm::length(rNodePos - listenerPos) / std::max(sr, 0.001f);
        const float lOpacity = distanceOpacity(lDistFrac);
        const float rOpacity = distanceOpacity(rDistFrac);

        const glm::vec3 leftColor  = theme.glStereoL;
        const glm::vec3 rightColor = theme.glStereoR;

        // Camera-space Z for depth sorting (more negative = farther from camera)
        const float lCamZ = (viewMatrix_ * glm::vec4(lNodePos, 1.0f)).z;
        const float rCamZ = (viewMatrix_ * glm::vec4(rNodePos, 1.0f)).z;

        if (lCamZ < rCamZ) {
            // L is farther — draw L first, then R on top
            drawSourceShape(lNodePos, 0.045f, leftColor, lOpacity, scene.sourceShape, now, scene.clusterCount);
            drawSourceShape(rNodePos, 0.045f, rightColor, rOpacity, scene.sourceShape, now, scene.clusterCount);
        } else {
            // R is farther — draw R first, then L on top
            drawSourceShape(rNodePos, 0.045f, rightColor, rOpacity, scene.sourceShape, now, scene.clusterCount);
            drawSourceShape(lNodePos, 0.045f, leftColor, lOpacity, scene.sourceShape, now, scene.clusterCount);
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
            // Reset wave emitter + RMS so new source gets fresh wave spawning
            foreignWaveEmitters_[ri] = WaveEmitter{};
            foreignWaveEmittersL_[ri] = WaveEmitter{};
            foreignWaveEmittersR_[ri] = WaveEmitter{};
            foreignSmoothedRms_[ri] = 0.0f;
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
            if (showSphere) {
                drawSphere(fPos, fsr, color, 0.03f);
                drawSphereWireframe(fsr, color, 0.02f, fPos);
            }

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
            if (fs.stereoWidth > 0.0f) {
                const glm::vec3 fL(sp.lx, sp.lz, -sp.ly);
                const glm::vec3 fR(sp.rx, sp.rz, -sp.ry);
                drawSoundWaves(foreignWaveEmittersL_[ri], fL, fsr, foreignSmoothedRms_[ri], color * 0.8f, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);
                drawSoundWaves(foreignWaveEmittersR_[ri], fR, fsr, foreignSmoothedRms_[ri], color * 0.8f, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);
            } else {
                drawSoundWaves(foreignWaveEmitters_[ri], fPos, fsr, foreignSmoothedRms_[ri], color, now, waveIntensity, waveBaseOpacity, waveSpeed, waveCount);
            }
        }
        glDepthMask(GL_TRUE);

        // Draw foreign source nodes on top
        for (int i = 0; i < fp.count; ++i) {
            const auto& fs = fp.sources[i];
            const auto color = kPalette[fs.colorIndex % 8];
            const int ri = std::clamp(i, 0, static_cast<int>(kMaxLinkedSources) - 1);
            const auto& sp = foreignSmoothedPos_[ri];
            const glm::vec3 fPos(sp.x, sp.z, -sp.y);
            drawSourceShape(fPos, 0.035f, color, 0.6f, fs.sourceShape, now, scene.clusterCount);
            // Highlight ring around focused foreign source
            if (fs.colorIndex == focusIdx)
                drawSphere(fPos, 0.050f, color, 0.25f);
            if (fs.stereoWidth > 0.0f) {
                const glm::vec3 fL(sp.lx, sp.lz, -sp.ly);
                const glm::vec3 fR(sp.rx, sp.rz, -sp.ry);
                drawSourceShape(fL, 0.028f, color * 0.8f, 0.45f, fs.sourceShape, now, scene.clusterCount);
                drawSourceShape(fR, 0.028f, color * 0.8f, 0.45f, fs.sourceShape, now, scene.clusterCount);
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
            const float lDistFracT = glm::length(lNodePos) / std::max(sr, 0.001f);
            const float rDistFracT = glm::length(rNodePos) / std::max(sr, 0.001f);
            const float lTrailOpacity = distanceOpacity(lDistFracT) * 0.5f;
            const float rTrailOpacity = distanceOpacity(rDistFracT) * 0.5f;

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
    if (scene.showLabels) {
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
    if (vaoConeWire_) { glDeleteVertexArrays(1, &vaoConeWire_); vaoConeWire_ = 0; }
    if (iboConeWire_) { glDeleteBuffers(1, &iboConeWire_);      iboConeWire_ = 0; }
    if (vaoArrow2D_) { glDeleteVertexArrays(1, &vaoArrow2D_); vaoArrow2D_ = 0; }
    if (vboArrow2D_) { glDeleteBuffers(1, &vboArrow2D_);      vboArrow2D_ = 0; }

    // Source shape meshes
    if (vaoPyramid_)     { glDeleteVertexArrays(1, &vaoPyramid_);     vaoPyramid_ = 0; }
    if (vboPyramid_)     { glDeleteBuffers(1, &vboPyramid_);          vboPyramid_ = 0; }
    if (iboPyramid_)     { glDeleteBuffers(1, &iboPyramid_);          iboPyramid_ = 0; }
    if (vaoPyramidWire_) { glDeleteVertexArrays(1, &vaoPyramidWire_); vaoPyramidWire_ = 0; }
    if (iboPyramidWire_) { glDeleteBuffers(1, &iboPyramidWire_);      iboPyramidWire_ = 0; }
    if (vaoCube_)        { glDeleteVertexArrays(1, &vaoCube_);        vaoCube_ = 0; }
    if (vboCube_)        { glDeleteBuffers(1, &vboCube_);             vboCube_ = 0; }
    if (iboCube_)        { glDeleteBuffers(1, &iboCube_);             iboCube_ = 0; }
    if (vaoCubeWire_)    { glDeleteVertexArrays(1, &vaoCubeWire_);    vaoCubeWire_ = 0; }
    if (iboCubeWire_)    { glDeleteBuffers(1, &iboCubeWire_);         iboCubeWire_ = 0; }
    if (vaoOcta_)        { glDeleteVertexArrays(1, &vaoOcta_);        vaoOcta_ = 0; }
    if (vboOcta_)        { glDeleteBuffers(1, &vboOcta_);             vboOcta_ = 0; }
    if (iboOcta_)        { glDeleteBuffers(1, &iboOcta_);             iboOcta_ = 0; }
    if (vaoOctaWire_)    { glDeleteVertexArrays(1, &vaoOctaWire_);    vaoOctaWire_ = 0; }
    if (iboOctaWire_)    { glDeleteBuffers(1, &iboOctaWire_);         iboOctaWire_ = 0; }
    if (vaoTorus_)       { glDeleteVertexArrays(1, &vaoTorus_);       vaoTorus_ = 0; }
    if (vboTorus_)       { glDeleteBuffers(1, &vboTorus_);            vboTorus_ = 0; }
    if (iboTorus_)       { glDeleteBuffers(1, &iboTorus_);            iboTorus_ = 0; }
    if (vaoTorusWire_)   { glDeleteVertexArrays(1, &vaoTorusWire_);   vaoTorusWire_ = 0; }
    if (iboTorusWire_)   { glDeleteBuffers(1, &iboTorusWire_);        iboTorusWire_ = 0; }

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

    skyboxShader_.reset();
    groundShader_.reset();
    if (vaoSkybox_) { glDeleteVertexArrays(1, &vaoSkybox_); vaoSkybox_ = 0; }
    if (vboSkybox_) { glDeleteBuffers(1, &vboSkybox_); vboSkybox_ = 0; }
    if (vaoGround_) { glDeleteVertexArrays(1, &vaoGround_); vaoGround_ = 0; }
    if (vboGround_) { glDeleteBuffers(1, &vboGround_); vboGround_ = 0; }
    if (iboGround_) { glDeleteBuffers(1, &iboGround_); iboGround_ = 0; }
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
        const int listW = 180;

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
            if (ownIsPilot_.load(std::memory_order_relaxed))
                selfName += " (Pilot)";
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
            if (fs.isPilot)
                name += " (Pilot)";

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
    } else if (headFollowsActive_ && accumulator_) {
        // Head-follows mode: quaternion accumulation for gimbal-lock-free rotation.
        constexpr float kSensitivity = 0.007f;  // ~0.4 deg/px
        const float dx = static_cast<float>(delta.x);
        const float dy = static_cast<float>(delta.y);

        auto* accum = accumulator_;
        auto alive = glAlive_;
        auto driving = accum->drivingFromInput;
        auto* pYaw   = cachedYawParam_;
        auto* pPitch = cachedPitchParam_;
        auto* pRoll  = cachedRollParam_;
        const bool needGestureBegin = !headFollowsGestureActive_;
        headFollowsGestureActive_ = true;
        juce::MessageManager::callAsync([accum, alive, driving, pYaw, pPitch, pRoll, dx, dy, needGestureBegin]() {
            if (!alive->load(std::memory_order_acquire)) return;
            // Begin automation gesture on first drag delta (DAWs require this for recording)
            if (needGestureBegin) {
                if (pYaw)   pYaw->beginChangeGesture();
                if (pPitch) pPitch->beginChangeGesture();
                if (pRoll)  pRoll->beginChangeGesture();
            }
            driving->store(true, std::memory_order_relaxed);
            accum->applyMouseDelta(dx, dy, kSensitivity);
            auto rpy = accum->bakeRPY();
            if (pYaw)   pYaw->setValueNotifyingHost(pYaw->convertTo0to1(rpy.yawDeg));
            if (pPitch) pPitch->setValueNotifyingHost(pPitch->convertTo0to1(rpy.pitchDeg));
            if (pRoll)  pRoll->setValueNotifyingHost(pRoll->convertTo0to1(rpy.rollDeg));
            driving->store(false, std::memory_order_relaxed);
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

    // End head-follows automation gesture
    if (headFollowsGestureActive_) {
        headFollowsGestureActive_ = false;
        auto* pYaw   = cachedYawParam_;
        auto* pPitch = cachedPitchParam_;
        auto* pRoll  = cachedRollParam_;
        auto alive = glAlive_;
        juce::MessageManager::callAsync([pYaw, pPitch, pRoll, alive]() {
            if (!alive->load(std::memory_order_acquire)) return;
            if (pYaw)   pYaw->endChangeGesture();
            if (pPitch) pPitch->endChangeGesture();
            if (pRoll)  pRoll->endChangeGesture();
        });
    }
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

    // Skybox shader (procedural sky)
    skyboxShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!skyboxShader_->addVertexShader(kSkyboxVertShader) ||
        !skyboxShader_->addFragmentShader(kSkyboxFragShader) ||
        !skyboxShader_->link()) {
        skyboxShader_.reset();
        jassertfalse;
    }

    // Ground plane shader (procedural ground)
    groundShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!groundShader_->addVertexShader(kGroundVertShader) ||
        !groundShader_->addFragmentShader(kGroundFragShader) ||
        !groundShader_->link()) {
        groundShader_.reset();
        jassertfalse;
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
// drawSphereWireframeWithModel — wireframe with arbitrary model matrix
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSphereWireframeWithModel(const glm::mat4& model,
                                                  const glm::vec3& color, float opacity)
{
    if (!lineShader_ || vaoSphereWire_ == 0 || sphereWireIndexCount_ == 0) return;

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
// Each wave spawns at the source's averaged position and expands from that fixed
// point. Waves do NOT follow the moving source — they emanate from where they
// were born.
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSoundWaves(WaveEmitter& emitter, const glm::vec3& center,
                                   float sphereRadius, float inputRms,
                                   const glm::vec3& color, double nowSeconds,
                                   float intensity, float baseOpacity,
                                   float speed, int numWaves)
{
    if (sphereRadius < 0.001f || intensity < 0.001f
        || baseOpacity < 0.0001f || numWaves < 1) return;

    // Update position average (also tracks velocity)
    const double dt = (emitter.lastSpawnTime > 0.0)
                      ? (nowSeconds - emitter.lastSpawnTime) : 0.0;
    emitter.updateAverage(center, dt);

    // Wave lifetime: time for a wave to expand from 0 to sphereRadius
    const float waveLifetime = (speed > 1e-5f) ? (1.0f / speed) : 100.0f;
    const float baseInterval = waveLifetime / static_cast<float>(numWaves);

    // Adaptive spawn rate: increase spawns when source moves fast so birth
    // positions stay close together.  speedRatio measures how far the source
    // travels per base spawn interval relative to the sphere radius — when
    // it exceeds ~0.15 the gaps become visible.
    const float travelPerSpawn = emitter.smoothedSpeed * baseInterval;
    const float gapRatio = (sphereRadius > 1e-5f) ? (travelPerSpawn / sphereRadius) : 0.0f;
    // spawnMul: 1x at rest, ramps up to 4x as gap grows, capped at 4x
    const float spawnMul = std::clamp(1.0f + gapRatio * 6.0f, 1.0f, 4.0f);
    const float spawnInterval = baseInterval / spawnMul;

    // Spawn new waves at regular intervals (only if there's audible signal)
    if (inputRms >= 0.001f) {
        if (emitter.lastSpawnTime < 0.0) {
            emitter.lastSpawnTime = nowSeconds;
            emitter.spawn(nowSeconds);
        } else {
            while (nowSeconds - emitter.lastSpawnTime >= spawnInterval) {
                emitter.lastSpawnTime += spawnInterval;
                emitter.spawn(emitter.lastSpawnTime);
            }
        }
    }

    // Volume-based opacity multiplier: 0dB (rms=1.0) → 2x, -6dB → 1x, -12dB → 0.5x
    const float rmsMul = std::clamp(inputRms * 2.0f, 0.0f, 2.0f);

    // Scale down per-wave opacity when spawning faster to keep total visual
    // energy roughly constant (more waves × less opacity ≈ same brightness)
    const float velocityOpacityMul = 1.0f / spawnMul;

    // Draw all live waves
    for (int i = 0; i < emitter.count; ++i) {
        const int idx = (emitter.head - i + WaveEmitter::kMaxWaves) % WaveEmitter::kMaxWaves;
        const auto& wave = emitter.waves[idx];

        const float age = static_cast<float>(nowSeconds - wave.birthTime);
        const float phase = age * speed;  // 0.0 = just born, 1.0 = at sphere boundary

        if (phase >= 1.0f || phase < 0.0f) continue;

        const float waveRadius = phase * sphereRadius;
        if (waveRadius < 0.005f) continue;

        float fadeIn  = std::clamp(phase * 5.0f, 0.0f, 1.0f);
        float fadeOut = 1.0f - phase;
        float waveOpacity = fadeIn * fadeOut * baseOpacity * intensity * rmsMul * velocityOpacityMul;

        if (waveOpacity < 0.001f) continue;

        const glm::mat4 model = glm::scale(
            glm::translate(glm::mat4(1.0f), wave.birthPos),
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

    // --- Cone wireframe VAO: shares vboCone_ data, uses separate line indices ---
    if (vaoConeWire_) glDeleteVertexArrays(1, &vaoConeWire_);
    if (iboConeWire_) glDeleteBuffers(1, &iboConeWire_);

    auto coneWireIdx = buildConeWireframe(16);
    coneWireIndexCount_ = static_cast<int>(coneWireIdx.size());

    glGenVertexArrays(1, &vaoConeWire_);
    glGenBuffers(1, &iboConeWire_);

    glBindVertexArray(vaoConeWire_);
    glBindBuffer(GL_ARRAY_BUFFER, vboCone_);

    const GLsizei coneWireStride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, coneWireStride, nullptr);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboConeWire_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(coneWireIdx.size() * sizeof(unsigned)),
                 coneWireIdx.data(), GL_STATIC_DRAW);

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
// drawConeWireframeWithModel — cone wireframe with arbitrary model matrix
// ---------------------------------------------------------------------------
void XYZPanGLView::drawConeWireframeWithModel(const glm::mat4& model,
                                                const glm::vec3& color, float opacity)
{
    if (!lineShader_ || vaoConeWire_ == 0 || coneWireIndexCount_ == 0) return;

    lineShader_->use();
    lineShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    lineShader_->setUniformMat4("model",      glm::value_ptr(model),       1, GL_FALSE);
    lineShader_->setUniform("lineColor", color.r, color.g, color.b);
    lineShader_->setUniform("opacity",   opacity);

    glBindVertexArray(vaoConeWire_);
    glDrawElements(GL_LINES, coneWireIndexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// uploadSourceShapeVAOs — pyramid, cube, octahedron, torus
// ---------------------------------------------------------------------------
void XYZPanGLView::uploadSourceShapeVAOs()
{
    // Helper: upload an indexed mesh (interleaved pos+normal) and its wireframe
    auto uploadIndexedMesh = [this](const SphereGeometry& geo, const std::vector<unsigned>& wireIdx,
                                     GLuint& vao, GLuint& vbo, GLuint& ibo, int& indexCount,
                                     GLuint& wireVao, GLuint& wireIbo, int& wireCount) {
        // Solid VAO
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(geo.vertices.size() * sizeof(float)),
                     geo.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(geo.indices.size() * sizeof(unsigned)),
                     geo.indices.data(), GL_STATIC_DRAW);
        indexCount = static_cast<int>(geo.indices.size());
        const GLsizei stride = 6 * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(3 * sizeof(float)));
        glBindVertexArray(0);

        // Wireframe VAO (shares VBO)
        glGenVertexArrays(1, &wireVao);
        glGenBuffers(1, &wireIbo);
        glBindVertexArray(wireVao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireIbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(wireIdx.size() * sizeof(unsigned)),
                     wireIdx.data(), GL_STATIC_DRAW);
        wireCount = static_cast<int>(wireIdx.size());
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    };

    // Pyramid
    {
        auto geo = buildPyramid();
        auto wire = buildPyramidWireframe();
        uploadIndexedMesh(geo, wire,
                          vaoPyramid_, vboPyramid_, iboPyramid_, pyramidIndexCount_,
                          vaoPyramidWire_, iboPyramidWire_, pyramidWireIndexCount_);
    }

    // Cube
    {
        auto geo = buildCube();
        auto wire = buildCubeWireframe();
        uploadIndexedMesh(geo, wire,
                          vaoCube_, vboCube_, iboCube_, cubeIndexCount_,
                          vaoCubeWire_, iboCubeWire_, cubeWireIndexCount_);
    }

    // Octahedron
    {
        auto geo = buildOctahedron();
        auto wire = buildOctahedronWireframe();
        uploadIndexedMesh(geo, wire,
                          vaoOcta_, vboOcta_, iboOcta_, octaIndexCount_,
                          vaoOctaWire_, iboOctaWire_, octaWireIndexCount_);
    }

    // Torus
    {
        auto geo = buildTorus(0.7f, 0.25f, 24, 12);
        auto wire = buildTorusWireframe(24, 12);
        uploadIndexedMesh(geo, wire,
                          vaoTorus_, vboTorus_, iboTorus_, torusIndexCount_,
                          vaoTorusWire_, iboTorusWire_, torusWireIndexCount_);
    }
}

// ---------------------------------------------------------------------------
// drawSourceShape — dispatches to the correct mesh based on shape enum
// ---------------------------------------------------------------------------
void XYZPanGLView::drawSourceShape(const glm::vec3& position, float radius,
                                    const glm::vec3& color, float opacity,
                                    int shape, double timeSeconds, int clusterCount)
{
    if (!sphereShader_) return;

    const float angle = static_cast<float>(std::fmod(timeSeconds * 45.0, 360.0));
    const float radAngle = glm::radians(angle);

    auto makeModel = [&](float scale) {
        return glm::scale(
            glm::rotate(
                glm::translate(glm::mat4(1.0f), position),
                radAngle, glm::vec3(0.0f, 1.0f, 0.0f)),
            glm::vec3(scale));
    };

    auto drawMeshAt = [&](GLuint vao, int idxCount, const glm::mat4& model) {
        if (vao == 0 || idxCount == 0) return;
        glBindVertexArray(vao);
        sphereShader_->setUniformMat4("model", glm::value_ptr(model), 1, GL_FALSE);
        sphereShader_->setUniform("nodeColor", color.r, color.g, color.b);
        sphereShader_->setUniform("opacity", opacity);
        sphereShader_->setUniform("edgeFade", 0.0f);
        glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(vaoSphere_);
    };

    auto drawMesh = [&](GLuint vao, int idxCount, float scale) {
        drawMeshAt(vao, idxCount, makeModel(scale));
    };

    auto drawWire = [&](GLuint wireVao, int wireCount, float scale) {
        if (!lineShader_ || wireVao == 0 || wireCount == 0) return;
        glm::mat4 model = makeModel(scale);
        lineShader_->use();
        lineShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
        lineShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
        lineShader_->setUniformMat4("model",      glm::value_ptr(model),       1, GL_FALSE);
        lineShader_->setUniform("lineColor", color.r, color.g, color.b);
        lineShader_->setUniform("opacity", opacity * 0.6f);
        glBindVertexArray(wireVao);
        glDrawElements(GL_LINES, wireCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        // Re-establish sphere shader for subsequent draws
        sphereShader_->use();
        sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
        sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
        const glm::vec3 ld = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
        sphereShader_->setUniform("lightDir", ld.x, ld.y, ld.z);
        glBindVertexArray(vaoSphere_);
    };

    // Chaotic cluster drawing — shared by all cluster variants.
    // Each particle has unique orbit inclination, speed, phase, and spin axis
    // to produce noisy, 3D-spread motion rather than a flat donut.
    auto drawCluster = [&](GLuint vao, int idxCount, bool isSphere) {
        constexpr int kMaxCluster = 7;
        const int num = std::clamp(clusterCount, 1, kMaxCluster);
        const float miniR = radius * 0.5f;
        const float orbitR = radius * 1.6f;

        // Per-particle deterministic "random" seeds for variety
        static constexpr float kPhaseOffsets[kMaxCluster]  = { 0.0f, 2.19f, 4.01f, 1.37f, 5.28f, 3.49f, 0.83f };
        static constexpr float kSpeedMuls[kMaxCluster]     = { 1.0f, 1.35f, 0.7f, 1.6f, 0.9f, 1.2f, 0.55f };
        static constexpr float kInclinations[kMaxCluster]  = { 0.0f, 0.8f, -0.6f, 1.3f, -1.1f, 0.4f, -0.9f };
        static constexpr float kBobFreqs[kMaxCluster]      = { 1.7f, 2.3f, 1.1f, 3.0f, 1.5f, 2.7f, 0.8f };
        static constexpr float kBobAmps[kMaxCluster]        = { 0.4f, 0.6f, 0.3f, 0.5f, 0.7f, 0.35f, 0.55f };
        static constexpr float kSpinSpeeds[kMaxCluster]     = { 1.0f, -1.5f, 0.8f, -0.6f, 1.3f, -1.1f, 0.7f };
        static constexpr float kSizeVary[kMaxCluster]       = { 1.0f, 0.75f, 1.15f, 0.85f, 1.1f, 0.7f, 0.95f };

        for (int i = 0; i < num; ++i) {
            const float t = static_cast<float>(timeSeconds * double(kSpeedMuls[i])) + kPhaseOffsets[i];
            const float orbitAngle = t * 1.2f;
            const float incl = kInclinations[i];

            // 3D orbit: rotate base XZ circle by inclination around X axis
            const float cx = orbitR * std::cos(orbitAngle);
            const float cz = orbitR * std::sin(orbitAngle);
            // Apply inclination: tilt the orbit plane
            const float oy = cz * std::sin(incl) + std::sin(t * kBobFreqs[i]) * orbitR * kBobAmps[i] * 0.3f;
            const float oz = cz * std::cos(incl);

            const glm::vec3 offset(cx, oy, oz);
            const float localAngle = static_cast<float>(timeSeconds) * kSpinSpeeds[i] * 3.0f;
            // Per-particle spin axis — tilted for variety
            const glm::vec3 spinAxis = glm::normalize(glm::vec3(
                std::sin(kPhaseOffsets[i]),
                1.0f,
                std::cos(kPhaseOffsets[i] * 1.7f)));
            const float particleSize = miniR * kSizeVary[i];

            if (isSphere) {
                drawSphere(position + offset, particleSize, color, opacity);
            } else {
                glm::mat4 model = glm::scale(
                    glm::rotate(
                        glm::translate(glm::mat4(1.0f), position + offset),
                        localAngle, spinAxis),
                    glm::vec3(particleSize));
                drawMeshAt(vao, idxCount, model);
            }
        }
    };

    switch (static_cast<SourceShape>(shape)) {
        default:
        case kShapeSphere:
            drawSphere(position, radius, color, opacity);
            break;

        case kShapePyramid:
            drawMesh(vaoPyramid_, pyramidIndexCount_, radius * 1.4f);
            break;

        case kShapeCube:
            drawMesh(vaoCube_, cubeIndexCount_, radius * 1.3f);
            break;

        case kShapeOctahedron:
            drawMesh(vaoOcta_, octaIndexCount_, radius * 1.0f);
            break;

        case kShapeRing:
            drawMesh(vaoTorus_, torusIndexCount_, radius * 1.2f);
            drawWire(vaoTorusWire_, torusWireIndexCount_, radius * 1.21f);
            break;

        case kShapeClusterSpheres:
            drawCluster(vaoSphere_, sphereIndexCount_, true);
            break;

        case kShapeClusterPyramids:
            drawCluster(vaoPyramid_, pyramidIndexCount_, false);
            break;

        case kShapeClusterCubes:
            drawCluster(vaoCube_, cubeIndexCount_, false);
            break;

        case kShapeClusterOctas:
            drawCluster(vaoOcta_, octaIndexCount_, false);
            break;

        case kShapeClusterRings:
            drawCluster(vaoTorus_, torusIndexCount_, false);
            break;
    }
}

// ---------------------------------------------------------------------------
// drawAvatarSphere / drawAvatarCone — body-type-aware rendering
// ---------------------------------------------------------------------------
void XYZPanGLView::drawAvatarSphere(const glm::mat4& model, const glm::vec3& color,
                                      float opacity, int bodyType)
{
    switch (static_cast<BodyType>(bodyType)) {
        case kBodyNone:
            break;
        case kBodyGrid:
            drawSphereWireframeWithModel(model, color, 0.6f * opacity);
            // Re-establish sphere shader after lineShader switch
            sphereShader_->use();
            sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
            sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
            { const glm::vec3 ld = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
              sphereShader_->setUniform("lightDir", ld.x, ld.y, ld.z); }
            glBindVertexArray(vaoSphere_);
            break;
        case kBodyGhost:
            // Semi-transparent filled sphere with rim edge-fade
            sphereShader_->setUniformMat4("model", glm::value_ptr(model), 1, GL_FALSE);
            sphereShader_->setUniform("nodeColor", color.r, color.g, color.b);
            sphereShader_->setUniform("opacity",   0.15f * opacity);
            sphereShader_->setUniform("edgeFade",  1.0f);
            glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, nullptr);
            // Wireframe overlay
            drawSphereWireframeWithModel(model, color, 0.25f * opacity);
            sphereShader_->use();
            sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
            sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
            { const glm::vec3 ld = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
              sphereShader_->setUniform("lightDir", ld.x, ld.y, ld.z); }
            glBindVertexArray(vaoSphere_);
            break;
        case kBodyGlass:
            drawSphereWithModel(model, color, 0.3f * opacity);
            break;
        default: // kBodySolid
            drawSphereWithModel(model, color, opacity);
            break;
    }
}

void XYZPanGLView::drawAvatarCone(const glm::mat4& model, const glm::vec3& color,
                                    float opacity, int bodyType)
{
    switch (static_cast<BodyType>(bodyType)) {
        case kBodyNone:
            break;
        case kBodyGrid:
            drawConeWireframeWithModel(model, color, 0.6f * opacity);
            // Re-establish sphere shader after lineShader switch
            sphereShader_->use();
            sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
            sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
            { const glm::vec3 ld = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
              sphereShader_->setUniform("lightDir", ld.x, ld.y, ld.z); }
            glBindVertexArray(vaoSphere_);
            break;
        case kBodyGhost:
            // Semi-transparent filled cone with low opacity + wireframe overlay
            drawCone(model, color, 0.15f * opacity);
            drawConeWireframeWithModel(model, color, 0.25f * opacity);
            sphereShader_->use();
            sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
            sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
            { const glm::vec3 ld = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));
              sphereShader_->setUniform("lightDir", ld.x, ld.y, ld.z); }
            glBindVertexArray(vaoSphere_);
            break;
        case kBodyGlass:
            drawCone(model, color, 0.3f * opacity);
            break;
        default: // kBodySolid
            drawCone(model, color, opacity);
            break;
    }
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
    drawAvatarCone(m, noseCol, 0.9f * alpha, avatar.bodyType);
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
    drawAvatarSphere(m, noseCol, 0.9f * alpha, avatar.bodyType);
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
    drawAvatarSphere(m, noseCol, 0.9f * alpha, avatar.bodyType);
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
    drawAvatarSphere(m, noseCol, 0.95f * alpha, avatar.bodyType);
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
    drawAvatarCone(m, noseCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarSphere(m, earCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarCone(m, earCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarSphere(m, earCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarCone(m, earCol, 0.9f * alpha, avatar.bodyType);
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
    drawAvatarCone(m, hatCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarSphere(m, hatCol, 0.9f * alpha, avatar.bodyType);
    }
    // Body — tall sphere above brim
    {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(0.0f, topY + 0.035f * hs * sz, 0.0f));
        m = glm::scale(m, glm::vec3(0.026f * hs * sz, 0.035f * hs * sz, 0.026f * hs * sz));
        drawAvatarSphere(m, hatCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarSphere(m, hatCol, 0.9f * alpha, avatar.bodyType);
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
    drawAvatarSphere(m, hatCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarCone(m, hatCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarSphere(m, hatCol, 0.9f * alpha, avatar.bodyType);
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
        drawAvatarSphere(m, hatCol, 0.85f * alpha, avatar.bodyType);
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
            drawAvatarSphere(m, eyeWhite, 0.95f * headAlpha, avatar.bodyType);
        }
        if (avatar.pupilSize > 0.0f) {
            glm::mat4 m = headRot;
            m = glm::translate(m, glm::vec3(side * spacing, eyeY, -eyeZ));
            m = glm::translate(m, glm::vec3(0.0f, 0.0f, -pupilForward));
            m = glm::scale(m, glm::vec3(pupilR));
            drawAvatarSphere(m, pupilColor, 0.95f * headAlpha, avatar.bodyType);
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
            drawAvatarSphere(m, eyeWhite, 0.95f * headAlpha, avatar.bodyType);
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
            drawAvatarSphere(m, pupilColor, 0.95f * headAlpha, avatar.bodyType);
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
            drawAvatarCone(m, xColor, 0.9f * headAlpha, avatar.bodyType);
        }
        // Diagonal 1 reverse
        {
            glm::mat4 m = base;
            m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            m = glm::scale(m, glm::vec3(cr * 0.4f, cl, cr * 0.4f));
            drawAvatarCone(m, xColor, 0.9f * headAlpha, avatar.bodyType);
        }
        // Diagonal 2: upper-left to lower-right (-45°)
        {
            glm::mat4 m = base;
            m = glm::rotate(m, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::scale(m, glm::vec3(cr * 0.4f, cl, cr * 0.4f));
            drawAvatarCone(m, xColor, 0.9f * headAlpha, avatar.bodyType);
        }
        // Diagonal 2 reverse
        {
            glm::mat4 m = base;
            m = glm::rotate(m, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::rotate(m, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            m = glm::scale(m, glm::vec3(cr * 0.4f, cl, cr * 0.4f));
            drawAvatarCone(m, xColor, 0.9f * headAlpha, avatar.bodyType);
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
        drawAvatarSphere(m, eyeWhite, 0.95f * headAlpha, avatar.bodyType);
    }
    if (avatar.pupilSize > 0.0f) {
        glm::mat4 m = headRot;
        m = glm::translate(m, glm::vec3(0.0f, eyeY, -eyeZ));
        m = glm::translate(m, glm::vec3(0.0f, 0.0f, -pupilForward));
        m = glm::scale(m, glm::vec3(pupilR));
        drawAvatarSphere(m, pupilColor, 0.95f * headAlpha, avatar.bodyType);
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
    const glm::vec3 listenerPos(snap.listenerPosX, snap.listenerPosZ, -snap.listenerPosY);

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
            glm::vec3 eyeWorld = headRot3 * eyeLocal + listenerPos;

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

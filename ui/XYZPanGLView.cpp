#include "XYZPanGLView.h"
#include "Shaders.h"
#include "Mesh.h"
#include "AlchemyLookAndFeel.h"
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
                             xyzpan::PositionBridge& bridge)
    : apvts_(apvts), proc_(proc), bridge_(bridge)
{
    // CRITICAL ORDER per RESEARCH.md: configure context BEFORE attachTo
    glContext_.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    glContext_.setRenderer(this);
    glContext_.setContinuousRepainting(true);
    glContext_.attachTo(*this);   // LAST
}

XYZPanGLView::~XYZPanGLView()
{
    // CRITICAL: detach FIRST in destructor to safely call openGLContextClosing()
    glContext_.detach();
}

// ---------------------------------------------------------------------------
// newOpenGLContextCreated — compile shaders, build geometry, upload to GPU
// ---------------------------------------------------------------------------
void XYZPanGLView::newOpenGLContextCreated()
{
    compileShaders();

    // Build room wireframe (half-size=1.0 → fits in [-1,1]^3 coordinate space)
    std::vector<float> roomVerts = buildRoomWireframe(1.0f);
    roomVertexCount_ = static_cast<int>(roomVerts.size()) / 3;
    uploadLineVAO(vaoRoom_, vboRoom_, roomVerts);

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
}

// ---------------------------------------------------------------------------
// renderOpenGL — called every frame on the GL thread
// ---------------------------------------------------------------------------
void XYZPanGLView::renderOpenGL()
{
    jassert(juce::OpenGLHelpers::isContextActive());

    // Viewport
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0) return;
    glViewport(0, 0, w, h);

    // Clear with alchemy background color
    juce::OpenGLHelpers::clear(juce::Colour(AlchemyLookAndFeel::kBackground));
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Update matrices
    viewMatrix_ = camera_.getViewMatrix();
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    projMatrix_ = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Read R parameter from APVTS to scale room/grid
    const float r = [&]() -> float {
        if (auto* a = apvts_.getRawParameterValue(kParamR))
            return a->load();
        return 1.0f;
    }();
    const glm::mat4 roomModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(r, r, r));

    // Read current source position from bridge (audio thread writes it)
    const auto snap = bridge_.read();
    // Coordinate convention mapping:
    //   XYZPan X = left/right  → GL X
    //   XYZPan Y = front/back  → GL -Z (GL +Z is toward viewer, +Y is front in XYZPan)
    //   XYZPan Z = up/down     → GL Y
    const glm::vec3 sourcePos(snap.x, snap.z, -snap.y);

    // Compute source opacity: full at close range, ~10% at max distance (sqrt(3)~1.73)
    const float maxDist = 1.732f;  // sqrt(3) = max distance in normalized cube
    const float distFrac = std::clamp(snap.distance / maxDist, 0.0f, 1.0f);
    const float sourceOpacity = 0.1f + 0.9f * (1.0f - distFrac);

    // Draw room wireframe — scaled by R (model = roomModelMatrix)
    {
        const glm::vec3 bronzeColor(0x8B / 255.0f, 0x5E / 255.0f, 0x2E / 255.0f);
        drawLines(vaoRoom_, roomVertexCount_, bronzeColor, 0.7f, roomModelMatrix);
    }

    // Draw floor grid — scaled by R (model = roomModelMatrix)
    {
        const glm::vec3 earthColor(0x3D / 255.0f, 0x2A / 255.0f, 0x10 / 255.0f);
        drawLines(vaoGrid_, gridVertexCount_, earthColor, 0.5f, roomModelMatrix);
    }

    // Draw listener node at origin — warm gold, always full opacity
    {
        const glm::vec3 listenerColor(0xC8 / 255.0f, 0xA8 / 255.0f, 0x6B / 255.0f);
        drawSphere(glm::vec3(0.0f), 0.045f, listenerColor, 1.0f);
    }

    // Draw source node at current position — bright gold, opacity based on distance
    {
        const glm::vec3 sourceColor = isSourceHovered_
            ? glm::vec3(0xFF / 255.0f, 0xD5 / 255.0f, 0x80 / 255.0f)  // hover: lighter gold
            : glm::vec3(0xE8 / 255.0f, 0xC4 / 255.0f, 0x6A / 255.0f); // normal: bright gold
        drawSphere(sourcePos, 0.06f, sourceColor, sourceOpacity);
    }

    // Cache projected source position for hit-testing on mouse events
    projectedSourcePos_ = projectToScreen(sourcePos);
}

// ---------------------------------------------------------------------------
// openGLContextClosing — delete GPU resources
// ---------------------------------------------------------------------------
void XYZPanGLView::openGLContextClosing()
{
    lineShader_.reset();
    sphereShader_.reset();

    if (vaoRoom_)   { glDeleteVertexArrays(1, &vaoRoom_);   vaoRoom_   = 0; }
    if (vboRoom_)   { glDeleteBuffers(1, &vboRoom_);        vboRoom_   = 0; }
    if (vaoGrid_)   { glDeleteVertexArrays(1, &vaoGrid_);   vaoGrid_   = 0; }
    if (vboGrid_)   { glDeleteBuffers(1, &vboGrid_);        vboGrid_   = 0; }
    if (vaoSphere_) { glDeleteVertexArrays(1, &vaoSphere_); vaoSphere_ = 0; }
    if (vboSphere_) { glDeleteBuffers(1, &vboSphere_);      vboSphere_ = 0; }
    if (iboSphere_) { glDeleteBuffers(1, &iboSphere_);      iboSphere_ = 0; }
}

// ---------------------------------------------------------------------------
// Mouse: Down
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseDown(const juce::MouseEvent& e)
{
    lastDragPos_ = e.getPosition();

    if (isNearSourceNode(e.getPosition())) {
        isDraggingSource_ = true;
        setMouseCursor(juce::MouseCursor(juce::MouseCursor::DraggingHandCursor));
    } else {
        isDraggingSource_ = false;
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
        // Camera orbit
        camera_.applyMouseDrag(static_cast<float>(delta.x),
                               static_cast<float>(delta.y));
    }
}

// ---------------------------------------------------------------------------
// Mouse: Up
// ---------------------------------------------------------------------------
void XYZPanGLView::mouseUp(const juce::MouseEvent& /*e*/)
{
    isDraggingSource_ = false;
    setMouseCursor(juce::MouseCursor(juce::MouseCursor::NormalCursor));
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

    // Sphere shader (listener + source nodes)
    sphereShader_ = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    if (!sphereShader_->addVertexShader(kSphereVertShader) ||
        !sphereShader_->addFragmentShader(kSphereFragShader) ||
        !sphereShader_->link()) {
        sphereShader_.reset();
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

    sphereShader_->use();

    const glm::mat4 model = glm::scale(
        glm::translate(glm::mat4(1.0f), position),
        glm::vec3(radius));

    // Light direction — fixed world-space direction for consistent shading
    const glm::vec3 lightDir = glm::normalize(glm::vec3(0.6f, 1.0f, 0.8f));

    sphereShader_->setUniformMat4("projection", glm::value_ptr(projMatrix_), 1, GL_FALSE);
    sphereShader_->setUniformMat4("view",       glm::value_ptr(viewMatrix_), 1, GL_FALSE);
    sphereShader_->setUniformMat4("model",      glm::value_ptr(model),       1, GL_FALSE);
    sphereShader_->setUniform("nodeColor", color.r,    color.g,    color.b);
    sphereShader_->setUniform("opacity",   opacity);
    sphereShader_->setUniform("lightDir",  lightDir.x, lightDir.y, lightDir.z);

    glBindVertexArray(vaoSphere_);
    glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, nullptr);
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

} // namespace xyzpan

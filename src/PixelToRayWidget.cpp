#include "PixelToRayWidget.h"
#include "FrameMathHelper.h"
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>

#include <cmath>
#include <cstddef>

namespace {

constexpr int kSphereSegments = 64;
constexpr int kGridSteps = 6;
constexpr int kPanoramaSegments = 144;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kHandleRadius = 8.0f;
constexpr float kHandlePointSize = 14.0f;
constexpr float kActiveHandlePointSize = 20.0f;
constexpr float kContourLineWidth = 2.4f;
constexpr float kActiveContourLineWidth = 3.6f;

bool isLockedComponent(FrameMath::ComponentId id) {
    return id == FrameMath::ComponentId::NegtiveZ;
}

FrameMath::ComponentId normalizedActiveComponent(FrameMath::ComponentId id) {
    return isLockedComponent(id) ? FrameMath::ComponentId::PositiveZ : id;
}

QString statusText(const FrameMath::State &state, const FrameMath::ComponentLengths &components, float zoom) {
    using FrameMath::componentKey;

    auto componentValue = [&](FrameMath::ComponentId id) {
        const QString fixedSuffix = isLockedComponent(id) ? QStringLiteral("  [fixed]") : QString();
        return QStringLiteral("%1: %2%3")
            .arg(componentKey(id))
            .arg(components[static_cast<int>(id)], 0, 'f', 2)
            .arg(fixedSuffix);
    };

    return QStringLiteral(
               "Qt rewrite: left-drag handle length\n"
               "active: %1  [x/X y/Y z/Z]\n"
               "lat: %2  [w/s]\n"
               "lon: %3  [a/d]\n"
               "roll: %4  [q/e]\n"
               "zoom: %5  [mouse wheel]\n"
               "center: %6  [right click menu]\n"
               "y trace: %7  [right click menu]\n"
               "x cylinder: %8  [right click menu]\n"
               "\n"
               "%9\n"
               "%10\n"
               "%11\n"
               "%12\n"
               "%13\n"
               "%14")
        .arg(FrameMath::componentKey(state.active))
        .arg(state.latitude, 0, 'f', 1)
        .arg(state.longitude, 0, 'f', 1)
        .arg(state.roll, 0, 'f', 1)
        .arg(zoom, 0, 'f', 2)
        .arg(state.fixGlobalCenter ? QStringLiteral("fixed") : QStringLiteral("move"))
        .arg(state.showPanoramaBand ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(state.showXTangentCylinder ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(componentValue(FrameMath::ComponentId::PositiveX))
        .arg(componentValue(FrameMath::ComponentId::NegtiveX))
        .arg(componentValue(FrameMath::ComponentId::PositiveY))
        .arg(componentValue(FrameMath::ComponentId::NegtiveY))
        .arg(componentValue(FrameMath::ComponentId::PositiveZ))
        .arg(componentValue(FrameMath::ComponentId::NegtiveZ));
}

QVector3D sphericalPoint(float radius, float latitude, float longitude) {
    return FrameMath::latLonToVector(latitude, longitude, radius);
}

void vectorToLatLon(const QVector3D &point, float &latitudeDeg, float &longitudeDeg) {
    const QVector3D normalized = point.normalized();
    latitudeDeg = std::asin(std::clamp(normalized.z(), -1.0f, 1.0f)) * 180.0f / kPi;
    longitudeDeg = std::atan2(normalized.y(), normalized.x()) * 180.0f / kPi;
}

}  // namespace

PixelToRayWidget::PixelToRayWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    m_components = {
        0.5f,
        0.3f,
        0.5f,
        0.3f,
        0.5f,
        FrameMath::kSphereRadius
    };
    m_state.active = normalizedActiveComponent(m_state.active);
}

PixelToRayWidget::~PixelToRayWidget() {
    makeCurrent();
    if (m_vertexBuffer.isCreated()) {
        m_vertexBuffer.destroy();
    }
    m_vao.destroy();
    m_program.reset();
    doneCurrent();
}

float PixelToRayWidget::latitude() const {
    return m_state.latitude;
}

float PixelToRayWidget::longitude() const {
    return m_state.longitude;
}

float PixelToRayWidget::roll() const {
    return m_state.roll;
}

float PixelToRayWidget::zoom() const {
    return m_zoom;
}

float PixelToRayWidget::capPolarAngle() const {
    return m_capPolarAngle;
}

float PixelToRayWidget::componentLength(FrameMath::ComponentId id) const {
    return m_components[static_cast<int>(id)];
}

QString PixelToRayWidget::activeComponentKey() const {
    return QString::fromLatin1(FrameMath::componentKey(m_state.active));
}

bool PixelToRayWidget::isGlobalCenterFixed() const {
    return m_state.fixGlobalCenter;
}

bool PixelToRayWidget::isPanoramaBandVisible() const {
    return m_state.showPanoramaBand;
}

bool PixelToRayWidget::isXTangentCylinderVisible() const {
    return m_state.showXTangentCylinder;
}

bool PixelToRayWidget::isLatitudeLocked() const {
    return m_locks.latitude;
}

bool PixelToRayWidget::isLongitudeLocked() const {
    return m_locks.longitude;
}

bool PixelToRayWidget::isRollLocked() const {
    return m_locks.roll;
}

bool PixelToRayWidget::isZoomLocked() const {
    return m_locks.zoom;
}

bool PixelToRayWidget::isCapPolarAngleLocked() const {
    return m_locks.capPolarAngle;
}

bool PixelToRayWidget::isComponentLocked(FrameMath::ComponentId id) const {
    return isComponentValueLocked(id);
}

void PixelToRayWidget::setLatitude(float value) {
    if (m_locks.latitude) {
        return;
    }
    m_state.latitude = value;
    clampStateToVisibleCap();
    emitStatus();
    update();
}

void PixelToRayWidget::setLongitude(float value) {
    if (m_locks.longitude) {
        return;
    }
    m_state.longitude = value;
    emitStatus();
    update();
}

void PixelToRayWidget::setRoll(float value) {
    if (m_locks.roll) {
        return;
    }
    m_state.roll = value;
    emitStatus();
    update();
}

void PixelToRayWidget::setZoom(float value) {
    if (m_locks.zoom) {
        return;
    }
    m_zoom = std::clamp(value, 0.35f, 2.5f);
    emitStatus();
    update();
}

void PixelToRayWidget::setCapPolarAngle(float value) {
    if (m_locks.capPolarAngle) {
        return;
    }
    m_capPolarAngle = std::clamp(value, 10.0f, 300.0f);
    clampStateToVisibleCap();
    emitStatus();
    update();
}

void PixelToRayWidget::setComponentLength(FrameMath::ComponentId id, float value) {
    if (isComponentValueLocked(id)) {
        return;
    }

    m_components[static_cast<int>(id)] = FrameMath::clampLength(value);
    emitStatus();
    update();
}

void PixelToRayWidget::setLatitudeLocked(bool locked) {
    m_locks.latitude = locked;
    emitStatus();
}

void PixelToRayWidget::setLongitudeLocked(bool locked) {
    m_locks.longitude = locked;
    emitStatus();
}

void PixelToRayWidget::setRollLocked(bool locked) {
    m_locks.roll = locked;
    emitStatus();
}

void PixelToRayWidget::setZoomLocked(bool locked) {
    m_locks.zoom = locked;
    emitStatus();
}

void PixelToRayWidget::setCapPolarAngleLocked(bool locked) {
    m_locks.capPolarAngle = locked;
    emitStatus();
}

void PixelToRayWidget::setComponentLocked(FrameMath::ComponentId id, bool locked) {
    if (isLockedComponent(id)) {
        return;
    }
    m_locks.components[static_cast<int>(id)] = locked;
    emitStatus();
}

void PixelToRayWidget::initializeGL() {
    initializeOpenGLFunctions();
    ensureGlResources();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    emitStatus();
}

void PixelToRayWidget::resizeGL(int w, int h) {
    // account for HiDPI / Retina: viewport must be set in device pixels (framebuffer size)
    const qreal dpr = devicePixelRatioF();
    const int fbw = std::max(1, static_cast<int>(std::round(w * dpr)));
    const int fbh = std::max(1, static_cast<int>(std::round(h * dpr)));
    glViewport(0, 0, fbw, fbh);

    updateViewProjection();
    emitStatus();
}


void PixelToRayWidget::paintGL() {
    updateViewProjection();
    //glViewport(0, 0, width(), height());
    glClearColor(0.06f, 0.10f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::vector<Vertex> surfaceVertices;
    std::vector<Vertex> lineVertices;
    std::vector<Vertex> activeLineVertices;
    std::vector<Vertex> pointVertices;
    std::vector<Vertex> activePointVertices;
    surfaceVertices.reserve(12000);
    lineVertices.reserve(12000);
    activeLineVertices.reserve(16);
    pointVertices.reserve(16);
    activePointVertices.reserve(2);

    appendSphereSurface(surfaceVertices);
    appendPanoramaBand(surfaceVertices, lineVertices);
    appendSphereWireframe(lineVertices);
    appendFrameGeometry(lineVertices, activeLineVertices, pointVertices);

    const QVector3D activeTip =
        currentOrigin() + FrameMath::componentDirection(currentFrame(), m_state.active) * m_components[static_cast<int>(m_state.active)];
    activePointVertices.push_back({activeTip, QVector4D(1.0f, 0.96f, 0.54f, 1.0f)});

    drawVertices(GL_TRIANGLES, surfaceVertices, 1.0f, 1.0f);
    drawVertices(GL_TRIANGLES, lineVertices, 1.0f, 1.0f);
    drawVertices(GL_TRIANGLES, activeLineVertices, 1.0f, 1.0f);
    drawVertices(GL_POINTS, pointVertices, 1.0f, kHandlePointSize);
    drawVertices(GL_POINTS, activePointVertices, 1.0f, kActiveHandlePointSize);
}

void PixelToRayWidget::mousePressEvent(QMouseEvent *event) {
    setFocus();

    if (event->button() == Qt::RightButton) {
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }

    FrameMath::ComponentId picked = m_state.active;
    if (tryPickHandle(event->position(), picked)) {
        if (isComponentValueLocked(picked)) {
            event->ignore();
            return;
        }

        m_state.active = picked;
        m_drag.active = true;
        m_drag.mode = DragState::Mode::ComponentLength;
        m_drag.component = picked;
        m_drag.startLength = m_components[static_cast<int>(picked)];
        m_drag.startMouse = event->position();

        emitStatus();
        update();
        return;
    }

    QVector3D spherePoint;
    if (!tryMapScreenToSphere(event->position(), spherePoint)) {
        event->ignore();
        return;
    }

    float latitude = m_state.latitude;
    float longitude = m_state.longitude;
    vectorToLatLon(spherePoint, latitude, longitude);
    if (!m_locks.latitude) {
        m_state.latitude = latitude;
    }
    if (!m_locks.longitude) {
        m_state.longitude = longitude;
    }
    m_drag.active = true;
    m_drag.mode = DragState::Mode::SurfaceTrack;
    m_drag.startMouse = event->position();

    emitStatus();
    update();
}

void PixelToRayWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!m_drag.active) {
        return;
    }

    if (m_drag.mode == DragState::Mode::SurfaceTrack) {
        updateSurfaceTracking(event->position());
    } else {
        updateDrag(event->position());
    }
    emitStatus();
    update();
}

void PixelToRayWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_drag.active) {
        m_drag.active = false;
        m_drag.mode = DragState::Mode::None;
        emitStatus();
        update();
    }
}

void PixelToRayWidget::wheelEvent(QWheelEvent *event) {
    if (m_locks.zoom) {
        event->ignore();
        return;
    }
    const qreal steps = event->angleDelta().y() / 120.0;
    m_zoom = std::clamp(m_zoom * std::pow(0.9f, static_cast<float>(steps)), 0.35f, 2.5f);
    emitStatus();
    update();
}

void PixelToRayWidget::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_X:
        m_state.active = (event->modifiers() & Qt::ShiftModifier)
                             ? FrameMath::ComponentId::NegtiveX
                             : FrameMath::ComponentId::PositiveX;
        break;
    case Qt::Key_Y:
        m_state.active = (event->modifiers() & Qt::ShiftModifier)
                             ? FrameMath::ComponentId::NegtiveY
                             : FrameMath::ComponentId::PositiveY;
        break;
    case Qt::Key_Z:
        m_state.active = (event->modifiers() & Qt::ShiftModifier)
                             ? m_state.active
                             : FrameMath::ComponentId::PositiveZ;
        break;
    case Qt::Key_W:
        if (!m_locks.latitude) {
            m_state.latitude = std::min(89.0f, m_state.latitude + 2.0f);
        }
        break;
    case Qt::Key_S:
        if (!m_locks.latitude) {
            m_state.latitude = std::max(-89.0f, m_state.latitude - 2.0f);
        }
        break;
    case Qt::Key_A:
        if (!m_locks.longitude) {
            m_state.longitude -= 2.0f;
        }
        break;
    case Qt::Key_D:
        if (!m_locks.longitude) {
            m_state.longitude += 2.0f;
        }
        break;
    case Qt::Key_Q:
        if (!m_locks.roll) {
            m_state.roll -= 3.0f;
        }
        break;
    case Qt::Key_E:
        if (!m_locks.roll) {
            m_state.roll += 3.0f;
        }
        break;
    case Qt::Key_Escape:
        window()->close();
        return;
    default:
        QOpenGLWidget::keyPressEvent(event);
        return;
    }

    clampStateToVisibleCap();
    emitStatus();
    update();
}

void PixelToRayWidget::contextMenuEvent(QContextMenuEvent *event) {
    showContextMenu(event->globalPos());
}

FrameMath::Frame PixelToRayWidget::currentFrame() const {
    return FrameMath::getLocalFrame(m_state.latitude, m_state.longitude, m_state.roll);
}

QVector3D PixelToRayWidget::currentOrigin() const {
    return FrameMath::latLonToVector(m_state.latitude, m_state.longitude, FrameMath::kSphereRadius);
}

QVector3D PixelToRayWidget::currentSceneCenter() const {
    return m_state.fixGlobalCenter ? QVector3D(0.0f, 0.0f, 0.0f) : currentOrigin();
}

QVector3D PixelToRayWidget::cameraPosition() const {
    return currentSceneCenter() + QVector3D(4.0f, 4.0f, 4.0f);
}

float PixelToRayWidget::effectiveCapPolarAngle() const {
    return std::min(m_capPolarAngle, 180.0f);
}

float PixelToRayWidget::minimumVisibleLatitude() const {
    return 90.0f - effectiveCapPolarAngle();
}

void PixelToRayWidget::clampStateToVisibleCap() {
    m_state.latitude = std::clamp(m_state.latitude, minimumVisibleLatitude(), 89.0f);
}

float PixelToRayWidget::pixelsPerUnit() const {
    return std::min(width(), height()) / (6.0f * m_zoom);
}

void PixelToRayWidget::updateViewProjection() {
    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    const float halfHeight = 3.0f * m_zoom;
    const float halfWidth = halfHeight * aspect;

    QMatrix4x4 projection;
    projection.ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 100.0f);

    QMatrix4x4 view;
    view.lookAt(cameraPosition(), currentSceneCenter(), QVector3D(0.0f, 0.0f, 1.0f));
    m_viewProjection = projection * view;
    const FrameMath::Frame frame = frameFromWidgetState();
    //cj-copilot: this is a new helper function that converts the current widget state (latitude, longitude, roll) into a Frame struct that contains the necessary information for rendering and interaction logic. You can implement this function to extract the frame data from the widget state and return it as a Frame object.
    // use 'frame' to compute handle positions, component directions, camera target, etc.
    // e.g. QVector3D dirPosX = FrameMath::componentDirection(frame, ComponentId::PositiveX);
    // ... rest of updateViewProjection ...
}

// c++
void PixelToRayWidget::ensureGlResources() {
    if (m_program) {
        return;
    }

    m_program.reset(new QOpenGLShaderProgram());

    // Vertex shader: use 'in' without layout qualifiers (GLSL 150)
    const char *vertexSrc = R"(
#version 150 core
in vec3 aPosition;
in vec4 aColor;
uniform mat4 uMvp;
uniform float uPointSize;
out vec4 vColor;
void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    gl_PointSize = uPointSize;
    vColor = aColor;
}
)";

    // Fragment shader: receives vColor from vertex stage
    const char *fragmentSrc = R"(
#version 150 core
in vec4 vColor;
uniform bool uRoundPoints;
out vec4 fragColor;
void main() {
    if (uRoundPoints) {
        vec2 p = gl_PointCoord * 2.0 - 1.0;
        if (dot(p, p) > 1.0) {
            discard;
        }
    }
    fragColor = vColor;
}
)";

    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSrc);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSrc);

    // Bind attribute locations before linking (matches glVertexAttribPointer usage)
    m_program->bindAttributeLocation("aPosition", 0);
    m_program->bindAttributeLocation("aColor", 1);

    if (!m_program->link()) {
        qWarning() << "Shader link failed:" << m_program->log();
        return;
    }

    m_vao.create();
    m_vao.bind();
    m_vertexBuffer.create();
    m_vertexBuffer.bind();
    m_vertexBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_program->bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
    m_program->release();
    m_vertexBuffer.release();
    m_vao.release();
}

void PixelToRayWidget::appendLine(std::vector<Vertex> &vertices,
                                  const QVector3D &a,
                                  const QVector3D &b,
                                  const QColor &color,
                                  float alpha,
                                  float widthPixels) const {
    const QVector4D rgba(color.redF(), color.greenF(), color.blueF(), alpha);
    const QPointF screenA = projectPoint(a).screen;
    const QPointF screenB = projectPoint(b).screen;
    const QVector2D screenDelta(screenB - screenA);
    if (screenDelta.lengthSquared() < 1e-6f) {
        return;
    }

    QVector3D viewForward = (currentSceneCenter() - cameraPosition()).normalized();
    QVector3D worldUp(0.0f, 0.0f, 1.0f);
    QVector3D cameraRight = QVector3D::crossProduct(viewForward, worldUp).normalized();
    if (cameraRight.lengthSquared() < 1e-6f) {
        worldUp = QVector3D(0.0f, 1.0f, 0.0f);
        cameraRight = QVector3D::crossProduct(viewForward, worldUp).normalized();
    }
    const QVector3D cameraUp = QVector3D::crossProduct(cameraRight, viewForward).normalized();

    const float halfWidthPixels = 0.5f * (widthPixels > 0.0f ? widthPixels : kContourLineWidth);
    const QVector2D perpendicular(-screenDelta.y(), screenDelta.x());
    const QVector2D screenNormal = perpendicular.normalized();
    const float worldUnitsPerPixel = 1.0f / std::max(1.0f, pixelsPerUnit());
    const QVector3D offset =
        (cameraRight * screenNormal.x() - cameraUp * screenNormal.y()) * (halfWidthPixels * worldUnitsPerPixel);

    const QVector3D aLeft = a - offset;
    const QVector3D aRight = a + offset;
    const QVector3D bLeft = b - offset;
    const QVector3D bRight = b + offset;

    vertices.push_back({aLeft, rgba});
    vertices.push_back({bLeft, rgba});
    vertices.push_back({bRight, rgba});
    vertices.push_back({aLeft, rgba});
    vertices.push_back({bRight, rgba});
    vertices.push_back({aRight, rgba});
}

void PixelToRayWidget::appendTriangle(std::vector<Vertex> &vertices, const QVector3D &a, const QVector3D &b, const QVector3D &c, const QColor &color, float alpha) const {
    const QVector4D rgba(color.redF(), color.greenF(), color.blueF(), alpha);
    vertices.push_back({a, rgba});
    vertices.push_back({b, rgba});
    vertices.push_back({c, rgba});
}

void PixelToRayWidget::appendSphereSurface(std::vector<Vertex> &vertices) const {
    const QVector3D center = currentSceneCenter();
    const QVector3D lightDir = QVector3D(0.4f, 0.6f, 1.0f).normalized();
    const int latBands = 28;
    const int lonBands = 48;
    const float minLatitude = minimumVisibleLatitude();

    auto shadedColor = [&](const QVector3D &point) {
        const QVector3D normal = (point - center).normalized();
        const float diffuse = std::max(0.0f, QVector3D::dotProduct(normal, lightDir));
        const float rim = std::pow(1.0f - std::max(0.0f, QVector3D::dotProduct(normal, (cameraPosition() - point).normalized())), 2.0f);
        const float shade = 0.18f + diffuse * 0.30f + rim * 0.22f;
        const QColor base(0x4f, 0x3f, 0x2f);
        return QColor::fromRgbF(
            std::min(1.0f, base.redF() + shade * 0.34f),
            std::min(1.0f, base.greenF() + shade * 0.26f),
            std::min(1.0f, base.blueF() + shade * 0.16f)
        );
    };

    for (int lat = 0; lat < latBands; ++lat) {
        const float lat0 = minLatitude + (90.0f - minLatitude) * static_cast<float>(lat) / static_cast<float>(latBands);
        const float lat1 = minLatitude + (90.0f - minLatitude) * static_cast<float>(lat + 1) / static_cast<float>(latBands);

        for (int lon = 0; lon < lonBands; ++lon) {
            const float lon0 = 360.0f * static_cast<float>(lon) / static_cast<float>(lonBands);
            const float lon1 = 360.0f * static_cast<float>(lon + 1) / static_cast<float>(lonBands);

            const QVector3D p00 = sphericalPoint(FrameMath::kSphereRadius, lat0, lon0);
            const QVector3D p01 = sphericalPoint(FrameMath::kSphereRadius, lat0, lon1);
            const QVector3D p10 = sphericalPoint(FrameMath::kSphereRadius, lat1, lon0);
            const QVector3D p11 = sphericalPoint(FrameMath::kSphereRadius, lat1, lon1);

            appendTriangle(vertices, p00, p10, p11, shadedColor((p00 + p10 + p11) / 3.0f), 0.78f);
            appendTriangle(vertices, p00, p11, p01, shadedColor((p00 + p11 + p01) / 3.0f), 0.78f);
        }
    }
}

void PixelToRayWidget::appendSphereWireframe(std::vector<Vertex> &vertices) const {
    const QVector3D center = currentSceneCenter();
    const QVector3D cameraDir = (cameraPosition() - center).normalized();
    const float minLatitude = minimumVisibleLatitude();

    auto alphaForSegment = [&](const QVector3D &a, const QVector3D &b) {
        const bool aFront = QVector3D::dotProduct(cameraDir, a - center) >= 0.0f;
        const bool bFront = QVector3D::dotProduct(cameraDir, b - center) >= 0.0f;
        if (aFront && bFront) {
            return 0.70f;
        }
        return m_state.showRearWireframe ? 0.22f : -1.0f;
    };

    const QColor sphereColor(0xe5, 0xe7, 0xeb);
    for (int ring = 0; ring <= kGridSteps * 2; ++ring) {
        const float latitude = minLatitude
            + (90.0f - minLatitude) * static_cast<float>(ring) / static_cast<float>(kGridSteps * 2);
        QVector3D previous = sphericalPoint(FrameMath::kSphereRadius, latitude, 0.0f);
        for (int segment = 1; segment <= kSphereSegments; ++segment) {
            const float longitude = (360.0f / static_cast<float>(kSphereSegments)) * static_cast<float>(segment);
            const QVector3D current = sphericalPoint(FrameMath::kSphereRadius, latitude, longitude);
            const float alpha = alphaForSegment(previous, current);
            if (alpha > 0.0f) {
                appendLine(vertices, previous, current, sphereColor, alpha);
            }
            previous = current;
        }
    }

    for (int meridian = 0; meridian < 8; ++meridian) {
        const float longitude = 45.0f * static_cast<float>(meridian);
        QVector3D previous = sphericalPoint(FrameMath::kSphereRadius, minLatitude, longitude);
        for (int segment = 1; segment <= kSphereSegments; ++segment) {
            const float latitude = minLatitude
                + (90.0f - minLatitude) * static_cast<float>(segment) / static_cast<float>(kSphereSegments);
            const QVector3D current = sphericalPoint(FrameMath::kSphereRadius, latitude, longitude);
            const float alpha = alphaForSegment(previous, current);
            if (alpha > 0.0f) {
                appendLine(vertices, previous, current, sphereColor, alpha);
            }
            previous = current;
        }
    }
}

void PixelToRayWidget::appendPanoramaBand(std::vector<Vertex> &surfaceVertices, std::vector<Vertex> &lineVertices) const {
    if (!m_state.showPanoramaBand && !m_state.showXTangentCylinder) {
        return;
    }

    const FrameMath::Frame frame = currentFrame();
    const QColor xPositiveTraceColor(0xff, 0xd1, 0x66);
    const QColor xNegativeTraceColor(0x7e, 0xc8, 0xff);
    const QColor yTraceColor(0x72, 0xf1, 0xb8);
    const QColor xPositiveCylinderFill(0xe6, 0x8a, 0x2e);
    const QColor xNegativeCylinderFill(0x2f, 0x8f, 0xd9);

    auto circlePoint = [&](const QVector3D &axisA, const QVector3D &axisB, float azimuthDeg, float radius) {
        const float radians = azimuthDeg * kPi / 180.0f;
        return (axisA * std::cos(radians) + axisB * std::sin(radians)).normalized() * radius;
    };

    auto appendGreatCircleTrace = [&](const QVector3D &axisA, const QVector3D &axisB, const QColor &color, float radius) {
        QVector3D previous = circlePoint(axisA, axisB, 0.0f, radius);
        for (int segment = 1; segment <= kPanoramaSegments; ++segment) {
            const float azimuth = 360.0f * static_cast<float>(segment) / static_cast<float>(kPanoramaSegments);
            const QVector3D current = circlePoint(axisA, axisB, azimuth, radius);
            appendLine(lineVertices, previous, current, color, 0.95f);
            previous = current;
        }
    };

    if (m_state.showXTangentCylinder) {
        const float xCylinderRadius =
            FrameMath::kSphereRadius + m_components[static_cast<int>(FrameMath::ComponentId::PositiveZ)];
        const float positiveY = m_components[static_cast<int>(FrameMath::ComponentId::PositiveY)];
        const float negativeY = m_components[static_cast<int>(FrameMath::ComponentId::NegtiveY)];
        //const float positiveHeight = std::sqrt(xCylinderRadius * xCylinderRadius + positiveY * positiveY);
        //const float negativeHeight = std::sqrt(xCylinderRadius * xCylinderRadius + negativeY * negativeY);
        const float positiveHeight =  positiveY * xCylinderRadius;
        const float negativeHeight = negativeY  * xCylinderRadius;

        for (int segment = 0; segment < kPanoramaSegments; ++segment) {
            const float azimuth0 = 360.0f * static_cast<float>(segment) / static_cast<float>(kPanoramaSegments);
            const float azimuth1 = 360.0f * static_cast<float>(segment + 1) / static_cast<float>(kPanoramaSegments);

            const QVector3D base0 = circlePoint(frame.zAxis, frame.xAxis, azimuth0, xCylinderRadius);
            const QVector3D base1 = circlePoint(frame.zAxis, frame.xAxis, azimuth1, xCylinderRadius);
            const QVector3D top0 = base0 + frame.yAxis * positiveHeight;
            const QVector3D top1 = base1 + frame.yAxis * positiveHeight;
            const QVector3D bottom0 = base0 - frame.yAxis * negativeHeight;
            const QVector3D bottom1 = base1 - frame.yAxis * negativeHeight;

            appendTriangle(surfaceVertices, base0, base1, top1, xPositiveCylinderFill, 0.34f);
            appendTriangle(surfaceVertices, base0, top1, top0, xPositiveCylinderFill, 0.34f);
            appendTriangle(surfaceVertices, bottom0, bottom1, base1, xNegativeCylinderFill, 0.34f);
            appendTriangle(surfaceVertices, bottom0, base1, base0, xNegativeCylinderFill, 0.34f);
            appendLine(lineVertices, top0, top1, xPositiveTraceColor, 0.78f);
            appendLine(lineVertices, bottom0, bottom1, xNegativeTraceColor, 0.78f);
        }

        appendGreatCircleTrace(frame.zAxis, frame.xAxis, xPositiveTraceColor, xCylinderRadius);
    }

    if (m_state.showPanoramaBand) {
        appendGreatCircleTrace(frame.zAxis, frame.yAxis, yTraceColor, FrameMath::kSphereRadius);
    }
}

void PixelToRayWidget::appendFrameGeometry(std::vector<Vertex> &lineVertices,
                                           std::vector<Vertex> &activeLineVertices,
                                           std::vector<Vertex> &pointVertices) const {
    const QVector3D origin = currentOrigin();
    const FrameMath::Frame frame = currentFrame();

    if (m_state.fixGlobalCenter) {
        appendLine(lineVertices, currentSceneCenter(), origin, QColor(0xff, 0xff, 0xff), 0.15f);
    }

    if (!m_state.showLocalFrame) {
        pointVertices.push_back({origin, QVector4D(1.0f, 1.0f, 1.0f, 1.0f)});
        return;
    }

    appendLine(lineVertices, origin, origin + frame.xAxis * FrameMath::kFrameVisualLength, FrameMath::axisColor('x'), 1.0f);
    appendLine(lineVertices, origin, origin + frame.yAxis * FrameMath::kFrameVisualLength, FrameMath::axisColor('y'), 1.0f);
    appendLine(lineVertices, origin, origin + frame.zAxis * FrameMath::kFrameVisualLength, FrameMath::axisColor('z'), 1.0f);

    for (int index = 0; index < static_cast<int>(FrameMath::ComponentId::Count); ++index) {
        const auto id = static_cast<FrameMath::ComponentId>(index);
        const QVector3D direction = FrameMath::componentDirection(frame, id);
        const FrameMath::ComponentMeta meta = FrameMath::componentMeta(id);
        const QColor color = FrameMath::axisColor(meta.axis);
        const bool isStaticXYGuide = meta.axis != 'z' && id != m_state.active;
        const float guideAlpha = id == m_state.active ? 1.0f : (isStaticXYGuide ? 0.28f : 0.65f);
        appendLine(lineVertices, origin, origin + direction * m_components[index], color, guideAlpha);
        pointVertices.push_back({origin + direction * m_components[index], QVector4D(color.redF(), color.greenF(), color.blueF(), 1.0f)});

        if (id == m_state.active) {
            appendLine(lineVertices, origin, origin + direction * m_components[index], QColor(0x0b, 0x12, 0x1d), 1.0f);
            appendLine(activeLineVertices,
                       origin,
                       origin + direction * m_components[index],
                       QColor(0xff, 0xf4, 0x8a),
                       0.95f,
                       kActiveContourLineWidth);
        }
    }

    pointVertices.push_back({origin, QVector4D(1.0f, 1.0f, 1.0f, 1.0f)});
}

void PixelToRayWidget::drawVertices(GLenum mode, const std::vector<Vertex> &vertices, float lineWidth, float pointSize) {
    if (vertices.empty()) {
        return;
    }

    ensureGlResources();
    m_vao.bind();
    m_vertexBuffer.bind();
    m_vertexBuffer.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(Vertex)));
    m_program->bind();
    m_program->setUniformValue("uMvp", m_viewProjection);
    m_program->setUniformValue("uPointSize", pointSize);
    m_program->setUniformValue("uRoundPoints", mode == GL_POINTS);
    if (mode == GL_TRIANGLES) {
        glDepthMask(GL_FALSE);
    } else {
        glLineWidth(lineWidth);
        glDepthMask(GL_TRUE);
    }
    glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size()));
    glDepthMask(GL_TRUE);
    m_program->release();
    m_vertexBuffer.release();
    m_vao.release();
}

PixelToRayWidget::ProjectedPoint PixelToRayWidget::projectPoint(const QVector3D &worldPoint) const {
    const QVector4D clip = m_viewProjection * QVector4D(worldPoint, 1.0f);
    const QVector3D ndc = clip.toVector3DAffine();
    return {
        QPointF((ndc.x() * 0.5f + 0.5f) * width(), (1.0f - (ndc.y() * 0.5f + 0.5f)) * height()),
        ndc.z()
    };
}

QVector2D PixelToRayWidget::projectVectorToScreen(const QVector3D &origin, const QVector3D &direction, float length) const {
    const QPointF a = projectPoint(origin).screen;
    const QPointF b = projectPoint(origin + direction * length).screen;
    return QVector2D(b - a);
}

PixelToRayWidget::HandleVisual PixelToRayWidget::handleVisual(FrameMath::ComponentId id) const {
    if (!m_state.showLocalFrame) {
        return {};
    }

    const_cast<PixelToRayWidget *>(this)->updateViewProjection();
    const QVector3D origin = currentOrigin();
    const QVector3D direction = FrameMath::componentDirection(currentFrame(), id);
    const QPointF center = projectPoint(
        origin + direction * m_components[static_cast<int>(id)]
    ).screen;

    return {center, kHandleRadius, true};
}

bool PixelToRayWidget::tryPickHandle(const QPointF &position, FrameMath::ComponentId &picked) const {
    return InteractionLogic::tryPickHandle(position, handleCandidates(), picked);
}

bool PixelToRayWidget::tryMapScreenToSphere(const QPointF &position, QVector3D &spherePoint) const {
    const_cast<PixelToRayWidget *>(this)->updateViewProjection();

    bool invertible = false;
    const QMatrix4x4 inverse = m_viewProjection.inverted(&invertible);
    if (!invertible || width() <= 0 || height() <= 0) {
        return false;
    }

    const float x = 2.0f * static_cast<float>(position.x()) / static_cast<float>(width()) - 1.0f;
    const float y = 1.0f - 2.0f * static_cast<float>(position.y()) / static_cast<float>(height());
    const QVector3D nearPoint = (inverse * QVector4D(x, y, -1.0f, 1.0f)).toVector3DAffine();
    const QVector3D farPoint = (inverse * QVector4D(x, y, 1.0f, 1.0f)).toVector3DAffine();
    const QVector3D rayDirection = (farPoint - nearPoint).normalized();

    const float a = QVector3D::dotProduct(rayDirection, rayDirection);
    const float b = 2.0f * QVector3D::dotProduct(nearPoint, rayDirection);
    const float c = QVector3D::dotProduct(nearPoint, nearPoint) - FrameMath::kSphereRadius * FrameMath::kSphereRadius;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return false;
    }

    const float root = std::sqrt(discriminant);
    const float t0 = (-b - root) / (2.0f * a);
    const float t1 = (-b + root) / (2.0f * a);
    const float t = t0 >= 0.0f ? t0 : t1;
    if (t < 0.0f) {
        return false;
    }

    spherePoint = nearPoint + rayDirection * t;
    float latitudeDeg = 0.0f;
    float longitudeDeg = 0.0f;
    vectorToLatLon(spherePoint, latitudeDeg, longitudeDeg);
    if (latitudeDeg < minimumVisibleLatitude()) {
        return false;
    }
    return true;
}

std::vector<InteractionLogic::HandleCandidate> PixelToRayWidget::handleCandidates() const {
    std::vector<InteractionLogic::HandleCandidate> candidates;
    candidates.reserve(static_cast<int>(FrameMath::ComponentId::Count));

    for (int index = 0; index < static_cast<int>(FrameMath::ComponentId::Count); ++index) {
        const auto id = static_cast<FrameMath::ComponentId>(index);
        const HandleVisual handle = handleVisual(id);
        candidates.push_back({id, handle.center, handle.radius, handle.visible && !isComponentValueLocked(id)});
    }

    return candidates;
}

void PixelToRayWidget::updateDrag(const QPointF &position) {
    if (isComponentValueLocked(m_drag.component)) {
        m_drag.active = false;
        return;
    }

    updateViewProjection();
    const QVector3D origin = currentOrigin();
    const QVector3D direction = FrameMath::componentDirection(currentFrame(), m_drag.component);
    const QVector2D axisScreen = projectVectorToScreen(origin, direction, 1.0f);
    const float deltaLength = InteractionLogic::lengthDeltaFromScreenDrag(m_drag.startMouse, position, axisScreen);
    m_components[static_cast<int>(m_drag.component)] = FrameMath::clampLength(m_drag.startLength + deltaLength);
    m_components[static_cast<int>(FrameMath::ComponentId::NegtiveZ)] = FrameMath::kSphereRadius;
}

void PixelToRayWidget::updateSurfaceTracking(const QPointF &position) {
    QVector3D spherePoint;
    if (!tryMapScreenToSphere(position, spherePoint)) {
        return;
    }

    float latitude = m_state.latitude;
    float longitude = m_state.longitude;
    vectorToLatLon(spherePoint, latitude, longitude);
    if (!m_locks.latitude) {
        m_state.latitude = latitude;
    }
    if (!m_locks.longitude) {
        m_state.longitude = longitude;
    }
    clampStateToVisibleCap();
}

void PixelToRayWidget::emitStatus() {
    m_state.active = normalizedActiveComponent(m_state.active);
    m_components[static_cast<int>(FrameMath::ComponentId::NegtiveZ)] = FrameMath::kSphereRadius;
    emit statusTextChanged(statusText(m_state, m_components, m_zoom));
}

void PixelToRayWidget::showContextMenu(const QPoint &globalPosition) {
    QMenu menu(this);
    QAction *rearAction = menu.addAction(tr("Rear hemisphere wireframe"));
    rearAction->setCheckable(true);
    rearAction->setChecked(m_state.showRearWireframe);

    QAction *frameAction = menu.addAction(tr("Local coordinate frame"));
    frameAction->setCheckable(true);
    frameAction->setChecked(m_state.showLocalFrame);

    QAction *panoramaAction = menu.addAction(tr("Y-axis great-circle trace"));
    panoramaAction->setCheckable(true);
    panoramaAction->setChecked(m_state.showPanoramaBand);

    QAction *xCylinderAction = menu.addAction(tr("X-axis tangential cylinder"));
    xCylinderAction->setCheckable(true);
    xCylinderAction->setChecked(m_state.showXTangentCylinder);

    QAction *centerAction = menu.addAction(tr("Global center (fix/move)"));
    centerAction->setCheckable(true);
    centerAction->setChecked(m_state.fixGlobalCenter);

    QAction *chosen = menu.exec(globalPosition);
    if (chosen == rearAction) {
        m_state.showRearWireframe = rearAction->isChecked();
    } else if (chosen == frameAction) {
        m_state.showLocalFrame = frameAction->isChecked();
    } else if (chosen == panoramaAction) {
        m_state.showPanoramaBand = panoramaAction->isChecked();
    } else if (chosen == xCylinderAction) {
        m_state.showXTangentCylinder = xCylinderAction->isChecked();
    } else if (chosen == centerAction) {
        m_state.fixGlobalCenter = centerAction->isChecked();
    }

    emitStatus();
    update();
}

FrameMath::Frame PixelToRayWidget::frameFromWidgetState() const {
    // get normal, east, north from spherical coords
    QVector3D normal, east, north;
    localFrameFromSpherical(m_state.latitude, m_state.longitude, normal, east, north);

    // apply roll to form xAxis (same convention used in FrameMath::rotateBasis)
    const float rollRad = m_state.roll * static_cast<float>(M_PI) / 180.0f;
    const float c = std::cos(rollRad);
    const float s = std::sin(rollRad);
    QVector3D xAxis = (east * c + north * s).normalized();

    // ensure right-handed orthonormal frame
    QVector3D zAxis = normal.normalized();
    QVector3D yAxis = QVector3D::crossProduct(zAxis, xAxis).normalized();

    return { xAxis, yAxis, zAxis };
}

bool PixelToRayWidget::isComponentValueLocked(FrameMath::ComponentId id) const {
    return m_locks.components[static_cast<int>(id)];
}
/* Example usage:
   QVector3D pos = sphericalToCartesian(20.0f, 35.0f, 1.0f);
   Spherical s = cartesianToSpherical(pos);
   QVector3D N, E, T;
   localFrameFromSpherical(20.0f, 35.0f, N, E, T);
*/

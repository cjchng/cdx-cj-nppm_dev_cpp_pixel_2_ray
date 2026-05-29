#include "PixelToRayWidget.h"
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QImage>
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
constexpr float kGlobalPositivePointSize = 16.0f;
constexpr float kGlobalNegativePointSize = 10.0f;
constexpr float kGlobalCenterPointSize = 18.0f;
constexpr float kGlobalAxisPickRadius = 10.0f;
constexpr float kContourLineWidth = 2.4f;
constexpr float kActiveContourLineWidth = 3.6f;
constexpr float kGlobalSpokeLineWidth = 1.4f;
constexpr float kSurfaceTrackStartThresholdPixels = 4.0f;
constexpr float kEntaniyaCapAngleDegrees = 140.0f;

float colorDistanceSquared(const QColor &a, const QColor &b) {
    const float dr = static_cast<float>(a.red() - b.red());
    const float dg = static_cast<float>(a.green() - b.green());
    const float db = static_cast<float>(a.blue() - b.blue());
    return dr * dr + dg * dg + db * db;
}

float catmullRom(float p0, float p1, float p2, float p3, float t) {
    const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    const float c = -0.5f * p0 + 0.5f * p2;
    const float d = p1;
    return ((a * t + b) * t + c) * t + d;
}

QColor globalTraceColor(char axis) {
    switch (axis) {
    case 'x':
        return QColor(0xff, 0xd8, 0x6b);
    case 'y':
        return QColor(0x77, 0xe7, 0xff);
    case 'z':
    default:
        return QColor(0xd2, 0xb7, 0xff);
    }
}

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
               "center: %6  [right click -> Global center (fix/move)]\n"
               "y trace: %7  [right click -> y-axis great-circle trace]\n"
               "x cylinder: %8  [right click -> x-axis tangential cylinder]\n"
               "depth hide: %9  [right click -> surface depth hiding]\n"
               "projection: %10  [toggle in right panel]\n"
               "\n"
                "%11\n"
                "%12\n"
                "%13\n"
                "%14\n"
               "%15\n"
               "%16")
        .arg(FrameMath::componentKey(state.active))
        .arg(state.latitude, 0, 'f', 1)
        .arg(state.longitude, 0, 'f', 1)
        .arg(state.roll, 0, 'f', 1)
        .arg(zoom, 0, 'f', 2)
        .arg(state.fixGlobalCenter ? QStringLiteral("fixed") : QStringLiteral("move"))
        .arg(state.showPanoramaBand ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(state.showXTangentCylinder ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(state.hideSurfaceDepth ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(state.perspectiveProjection ? QStringLiteral("perspective") : QStringLiteral("orthographic"))
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

float signedRollDegrees(const QVector3D &xAxis, float latitudeDeg, float longitudeDeg) {
    const FrameMath::Frame baseFrame = FrameMath::getLocalFrame(latitudeDeg, longitudeDeg, 0.0f);
    const float eastComponent = QVector3D::dotProduct(xAxis.normalized(), baseFrame.xAxis);
    const float northComponent = QVector3D::dotProduct(xAxis.normalized(), baseFrame.yAxis);
    return std::atan2(northComponent, eastComponent) * 180.0f / kPi;
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
        m_globeRadius
    };
    m_state.active = normalizedActiveComponent(m_state.active);

    const QStringList imageFileNames = {
        QStringLiteral("circular.png"),
        QStringLiteral("Entaniya 280-Facing Up.jpg")
    };
    for (const QString &imageFileName : imageFileNames) {
        const QStringList candidates = {
            QDir::current().absoluteFilePath(imageFileName),
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../") + imageFileName),
            QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../") + imageFileName)
        };
        for (const QString &path : candidates) {
            if (loadProjectionImage(path)) {
                return;
            }
        }
    }
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

float PixelToRayWidget::globeRadius() const {
    return m_globeRadius;
}

QQuaternion PixelToRayWidget::globeOrientation() const {
    return m_globeOrientation;
}

QString PixelToRayWidget::fisheyeImagePath() const {
    return m_projectionImagePath;
}

float PixelToRayWidget::perspectiveFovY() const {
    return m_perspectiveFovY;
}

float PixelToRayWidget::cameraEyeOffsetX() const {
    return m_cameraEyeOffset.x();
}

float PixelToRayWidget::cameraEyeOffsetY() const {
    return m_cameraEyeOffset.y();
}

float PixelToRayWidget::cameraEyeOffsetZ() const {
    return m_cameraEyeOffset.z();
}

float PixelToRayWidget::cameraTargetX() const {
    return cameraTarget().x();
}

float PixelToRayWidget::cameraTargetY() const {
    return cameraTarget().y();
}

float PixelToRayWidget::cameraTargetZ() const {
    return cameraTarget().z();
}

float PixelToRayWidget::cameraUpX() const {
    return m_cameraUpVector.x();
}

float PixelToRayWidget::cameraUpY() const {
    return m_cameraUpVector.y();
}

float PixelToRayWidget::cameraUpZ() const {
    return m_cameraUpVector.z();
}

float PixelToRayWidget::anchorGlobalX() const {
    return currentOrigin().x();
}

float PixelToRayWidget::anchorGlobalY() const {
    return currentOrigin().y();
}

float PixelToRayWidget::anchorGlobalZ() const {
    return currentOrigin().z();
}

float PixelToRayWidget::componentLength(FrameMath::ComponentId id) const {
    return m_components[static_cast<int>(id)];
}

QString PixelToRayWidget::activeComponentKey() const {
    return m_hasActiveComponent
        ? QString::fromLatin1(FrameMath::componentKey(m_state.active))
        : QStringLiteral("none");
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

bool PixelToRayWidget::isSurfaceDepthHidingEnabled() const {
    return m_state.hideSurfaceDepth;
}

bool PixelToRayWidget::isPerspectiveProjection() const {
    return m_state.perspectiveProjection;
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

bool PixelToRayWidget::isGlobeRadiusLocked() const {
    return m_locks.globeRadius;
}

bool PixelToRayWidget::isCameraEyeOffsetXLocked() const {
    return m_locks.cameraEyeOffsetX;
}

bool PixelToRayWidget::isCameraEyeOffsetYLocked() const {
    return m_locks.cameraEyeOffsetY;
}

bool PixelToRayWidget::isCameraEyeOffsetZLocked() const {
    return m_locks.cameraEyeOffsetZ;
}

bool PixelToRayWidget::isCameraUpXLocked() const {
    return m_locks.cameraUpX;
}

bool PixelToRayWidget::isCameraUpYLocked() const {
    return m_locks.cameraUpY;
}

bool PixelToRayWidget::isCameraUpZLocked() const {
    return m_locks.cameraUpZ;
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

void PixelToRayWidget::setGlobeRadius(float value) {
    if (m_locks.globeRadius) {
        return;
    }
    m_globeRadius = std::max(0.01f, value);
    m_components[static_cast<int>(FrameMath::ComponentId::NegtiveZ)] = m_globeRadius;
    emitStatus();
    update();
}

void PixelToRayWidget::setGlobeOrientation(const QQuaternion &orientation) {
    m_globeOrientation = orientation.normalized();
    emitStatus();
    update();
}

bool PixelToRayWidget::setFisheyeImagePath(const QString &path) {
    if (path.isEmpty()) {
        m_projectionImage = QImage();
        m_projectionImagePath.clear();
        m_projectionCircleCenter = QPointF();
        m_projectionCircleRadius = 0.0f;
        emitStatus();
        update();
        return true;
    }
    if (!loadProjectionImage(path)) {
        return false;
    }
    emitStatus();
    update();
    return true;
}

void PixelToRayWidget::setPerspectiveFovY(float value) {
    m_perspectiveFovY = std::clamp(value, 5.0f, 140.0f);
    emitStatus();
    update();
}

void PixelToRayWidget::setCameraEyeOffsetX(float value) {
    if (m_locks.cameraEyeOffsetX) {
        return;
    }
    m_cameraEyeOffset.setX(value);
    emitStatus();
    update();
}

void PixelToRayWidget::setCameraEyeOffsetY(float value) {
    if (m_locks.cameraEyeOffsetY) {
        return;
    }
    m_cameraEyeOffset.setY(value);
    emitStatus();
    update();
}

void PixelToRayWidget::setCameraEyeOffsetZ(float value) {
    if (m_locks.cameraEyeOffsetZ) {
        return;
    }
    m_cameraEyeOffset.setZ(value);
    emitStatus();
    update();
}

void PixelToRayWidget::setCameraUpX(float value) {
    if (m_locks.cameraUpX) {
        return;
    }
    m_cameraUpVector.setX(value);
    emitStatus();
    update();
}

void PixelToRayWidget::setCameraUpY(float value) {
    if (m_locks.cameraUpY) {
        return;
    }
    m_cameraUpVector.setY(value);
    emitStatus();
    update();
}

void PixelToRayWidget::setCameraUpZ(float value) {
    if (m_locks.cameraUpZ) {
        return;
    }
    m_cameraUpVector.setZ(value);
    emitStatus();
    update();
}

void PixelToRayWidget::setGlobeRadiusLocked(bool locked) {
    m_locks.globeRadius = locked;
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

void PixelToRayWidget::setGlobalCenterFixed(bool fixed) {
    m_state.fixGlobalCenter = fixed;
    emitStatus();
    update();
}

void PixelToRayWidget::setPanoramaBandVisible(bool visible) {
    m_state.showPanoramaBand = visible;
    emitStatus();
    update();
}

void PixelToRayWidget::setXTangentCylinderVisible(bool visible) {
    m_state.showXTangentCylinder = visible;
    emitStatus();
    update();
}

void PixelToRayWidget::setSurfaceDepthHidingEnabled(bool enabled) {
    m_state.hideSurfaceDepth = enabled;
    emitStatus();
    update();
}

void PixelToRayWidget::setPerspectiveProjection(bool enabled) {
    m_state.perspectiveProjection = enabled;
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

void PixelToRayWidget::setCameraEyeOffsetXLocked(bool locked) {
    m_locks.cameraEyeOffsetX = locked;
    emitStatus();
}

void PixelToRayWidget::setCameraEyeOffsetYLocked(bool locked) {
    m_locks.cameraEyeOffsetY = locked;
    emitStatus();
}

void PixelToRayWidget::setCameraEyeOffsetZLocked(bool locked) {
    m_locks.cameraEyeOffsetZ = locked;
    emitStatus();
}

void PixelToRayWidget::setCameraUpXLocked(bool locked) {
    m_locks.cameraUpX = locked;
    emitStatus();
}

void PixelToRayWidget::setCameraUpYLocked(bool locked) {
    m_locks.cameraUpY = locked;
    emitStatus();
}

void PixelToRayWidget::setCameraUpZLocked(bool locked) {
    m_locks.cameraUpZ = locked;
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

    std::vector<Vertex> globeSurfaceVertices;
    std::vector<Vertex> cylinderSurfaceVertices;
    std::vector<Vertex> lineVertices;
    std::vector<Vertex> activeLineVertices;
    std::vector<Vertex> pointVertices;
    std::vector<Vertex> globalPositivePointVertices;
    std::vector<Vertex> globalNegativePointVertices;
    std::vector<Vertex> globalCenterPointVertices;
    std::vector<Vertex> activePointVertices;
    globeSurfaceVertices.reserve(m_projectionImage.isNull() ? 10000 : 120000);
    cylinderSurfaceVertices.reserve(3000);
    lineVertices.reserve(12000);
    activeLineVertices.reserve(16);
    pointVertices.reserve(16);
    globalPositivePointVertices.reserve(6);
    globalNegativePointVertices.reserve(6);
    globalCenterPointVertices.reserve(1);
    activePointVertices.reserve(2);

    appendSphereSurface(globeSurfaceVertices);
    appendPanoramaBand(cylinderSurfaceVertices, lineVertices);
    appendSphereWireframe(lineVertices);
    appendGlobalAxisTraces(lineVertices, globalPositivePointVertices, globalNegativePointVertices, globalCenterPointVertices);
    appendFrameGeometry(lineVertices, activeLineVertices, pointVertices);

    if (m_state.showLocalFrame) {
        if (m_hasActiveComponent) {
            const QVector3D activeTip =
                currentOrigin() + FrameMath::componentDirection(currentFrame(), m_state.active) * m_components[static_cast<int>(m_state.active)];
            activePointVertices.push_back({activeTip, QVector4D(1.0f, 0.96f, 0.54f, 1.0f)});
        }
    }

    // Render scene surfaces first with depth writes enabled.
    drawVertices(GL_TRIANGLES,
                 globeSurfaceVertices,
                 1.0f,
                 1.0f,
                 m_state.hideSurfaceDepth,
                 m_state.hideSurfaceDepth);
    drawVertices(GL_TRIANGLES,
                 cylinderSurfaceVertices,
                 1.0f,
                 1.0f,
                 m_state.hideSurfaceDepth,
                 m_state.hideSurfaceDepth);

    // Render guides and interactive handles as a deliberate overlay pass.
    drawVertices(GL_TRIANGLES, lineVertices, 1.0f, 1.0f, false, false);
    drawVertices(GL_TRIANGLES, activeLineVertices, 1.0f, 1.0f, false, false);
    drawVertices(GL_POINTS, globalNegativePointVertices, 1.0f, kGlobalNegativePointSize, false, false);
    drawVertices(GL_POINTS, globalPositivePointVertices, 1.0f, kGlobalPositivePointSize, false, false);
    drawVertices(GL_POINTS, globalCenterPointVertices, 1.0f, kGlobalCenterPointSize, false, false);
    drawVertices(GL_POINTS, pointVertices, 1.0f, kHandlePointSize, false, false);
    drawVertices(GL_POINTS, activePointVertices, 1.0f, kActiveHandlePointSize, false, false);
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

    const bool shiftHeld = (event->modifiers() & Qt::ShiftModifier) != 0;

    if (shiftHeld) {
        QVector3D pickedWorldPoint;
        bool preferRearHemisphere = false;
        if (!tryPickGlobalAxisEndpoint(event->position(), pickedWorldPoint, preferRearHemisphere)) {
            event->ignore();
            return;
        }
        m_hasActiveComponent = false;
        m_drag.active = true;
        m_drag.mode = DragState::Mode::GlobalAxisTrack;
        m_drag.startOrientation = m_globeOrientation;
        m_drag.grabbedWorldPoint = pickedWorldPoint.normalized();
        m_drag.startMouse = event->position();
        m_drag.preferRearHemisphere = preferRearHemisphere;

        emitStatus();
        update();
        return;
    }

    FrameMath::ComponentId picked = m_state.active;
    if (tryPickHandle(event->position(), picked)) {
        if (isComponentValueLocked(picked)) {
            event->ignore();
            return;
        }

        m_state.active = picked;
        m_hasActiveComponent = true;
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

    m_hasActiveComponent = false;
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
        const QPointF delta = event->position() - m_drag.startMouse;
        const float distanceSquared =
            static_cast<float>(delta.x() * delta.x() + delta.y() * delta.y());
        if (distanceSquared < kSurfaceTrackStartThresholdPixels * kSurfaceTrackStartThresholdPixels) {
            return;
        }
        updateSurfaceTracking(event->position());
    } else if (m_drag.mode == DragState::Mode::GlobalAxisTrack) {
        updateGlobalAxisTracking(event->position());
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
        m_hasActiveComponent = true;
        break;
    case Qt::Key_Y:
        m_state.active = (event->modifiers() & Qt::ShiftModifier)
                             ? FrameMath::ComponentId::NegtiveY
                             : FrameMath::ComponentId::PositiveY;
        m_hasActiveComponent = true;
        break;
    case Qt::Key_Z:
        m_state.active = (event->modifiers() & Qt::ShiftModifier)
                             ? m_state.active
                             : FrameMath::ComponentId::PositiveZ;
        m_hasActiveComponent = true;
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
    const FrameMath::Frame localFrame = FrameMath::getLocalFrame(m_state.latitude, m_state.longitude, m_state.roll);
    return {
        m_globeOrientation.rotatedVector(localFrame.xAxis).normalized(),
        m_globeOrientation.rotatedVector(localFrame.yAxis).normalized(),
        m_globeOrientation.rotatedVector(localFrame.zAxis).normalized()
    };
}

QVector3D PixelToRayWidget::currentOrigin() const {
    return m_globeOrientation.rotatedVector(
        FrameMath::latLonToVector(m_state.latitude, m_state.longitude, m_globeRadius)
    );
}

QVector3D PixelToRayWidget::currentSceneCenter() const {
    return m_state.fixGlobalCenter ? QVector3D(0.0f, 0.0f, 0.0f) : currentOrigin();
}

QVector3D PixelToRayWidget::cameraTarget() const {
    return currentSceneCenter();
}

QVector3D PixelToRayWidget::cameraPosition() const {
    return cameraTarget() + m_cameraEyeOffset;
}

QVector3D PixelToRayWidget::cameraUp() const {
    if (m_cameraUpVector.lengthSquared() < 1e-6f) {
        return QVector3D(0.0f, 0.0f, 1.0f);
    }
    return m_cameraUpVector.normalized();
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
    if (m_state.perspectiveProjection) {
        const QVector3D target = cameraTarget();
        const QVector3D eyeToTarget = target - cameraPosition();
        const float distance = std::max(0.1f, eyeToTarget.length());
        const float halfFovRadians = 0.5f * qDegreesToRadians(m_perspectiveFovY);
        const float halfHeightWorld = std::tan(halfFovRadians) * distance;
        return height() > 0 ? static_cast<float>(height()) / (2.0f * halfHeightWorld) : 1.0f;
    }
    return std::min(width(), height()) / (6.0f * m_zoom);
}

void PixelToRayWidget::updateViewProjection() {
    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    QMatrix4x4 projection;
    if (m_state.perspectiveProjection) {
        projection.perspective(m_perspectiveFovY, aspect, 0.1f, 100.0f);
    } else {
        const float halfHeight = 3.0f * m_zoom;
        const float halfWidth = halfHeight * aspect;
        projection.ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, 0.1f, 100.0f);
    }

    QMatrix4x4 view;
    view.lookAt(cameraPosition(), cameraTarget(), cameraUp());
    m_viewProjection = projection * view;
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

    QVector3D viewForward = (cameraTarget() - cameraPosition()).normalized();
    QVector3D worldUp = cameraUp();
    QVector3D cameraRight = QVector3D::crossProduct(viewForward, worldUp).normalized();
    if (cameraRight.lengthSquared() < 1e-6f) {
        worldUp = QVector3D(0.0f, 1.0f, 0.0f);
        cameraRight = QVector3D::crossProduct(viewForward, worldUp).normalized();
    }
    const QVector3D cameraUp = QVector3D::crossProduct(cameraRight, viewForward).normalized();

    const float halfWidthPixels = 0.5f * (widthPixels > 0.0f ? widthPixels : kContourLineWidth);
    const QVector2D perpendicular(-screenDelta.y(), screenDelta.x());
    const QVector2D screenNormal = perpendicular.normalized();
    float worldUnitsPerPixel = 1.0f / std::max(1.0f, pixelsPerUnit());
    if (m_state.perspectiveProjection) {
        const QVector3D midpoint = 0.5f * (a + b);
        const QVector3D viewForward = (cameraTarget() - cameraPosition()).normalized();
        const float depthAlongView = std::max(0.1f, QVector3D::dotProduct(midpoint - cameraPosition(), viewForward));
        const float halfFovRadians = 0.5f * qDegreesToRadians(m_perspectiveFovY);
        const float halfHeightWorld = std::tan(halfFovRadians) * depthAlongView;
        worldUnitsPerPixel = (2.0f * halfHeightWorld) / std::max(1, height());
    }
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

QVector3D PixelToRayWidget::screenSpaceOffsetAtPoint(const QVector3D &point, float dxPixels, float dyPixels) const {
    QVector3D viewForward = (cameraTarget() - cameraPosition()).normalized();
    QVector3D worldUp = cameraUp();
    QVector3D cameraRight = QVector3D::crossProduct(viewForward, worldUp).normalized();
    if (cameraRight.lengthSquared() < 1e-6f) {
        worldUp = QVector3D(0.0f, 1.0f, 0.0f);
        cameraRight = QVector3D::crossProduct(viewForward, worldUp).normalized();
    }
    const QVector3D cameraUpVector = QVector3D::crossProduct(cameraRight, viewForward).normalized();

    float worldUnitsPerPixel = 1.0f / std::max(1.0f, pixelsPerUnit());
    if (m_state.perspectiveProjection) {
        const float depthAlongView = std::max(0.1f, QVector3D::dotProduct(point - cameraPosition(), viewForward));
        const float halfFovRadians = 0.5f * qDegreesToRadians(m_perspectiveFovY);
        const float halfHeightWorld = std::tan(halfFovRadians) * depthAlongView;
        worldUnitsPerPixel = (2.0f * halfHeightWorld) / std::max(1, height());
    }

    return cameraRight * (dxPixels * worldUnitsPerPixel)
        - cameraUpVector * (dyPixels * worldUnitsPerPixel);
}

void PixelToRayWidget::appendGlobalTerminalMarker(std::vector<Vertex> &lineVertices,
                                                  const QVector3D &point,
                                                  const QColor &color,
                                                  bool positive) const {
    const float markerRadius = positive ? 7.0f : 6.0f;
    const float alpha = positive ? 0.96f : 0.84f;

    if (positive) {
        const QVector3D left = point + screenSpaceOffsetAtPoint(point, -markerRadius, 0.0f);
        const QVector3D right = point + screenSpaceOffsetAtPoint(point, markerRadius, 0.0f);
        const QVector3D top = point + screenSpaceOffsetAtPoint(point, 0.0f, -markerRadius);
        const QVector3D bottom = point + screenSpaceOffsetAtPoint(point, 0.0f, markerRadius);
        appendLine(lineVertices, left, top, color, alpha, kGlobalSpokeLineWidth);
        appendLine(lineVertices, top, right, color, alpha, kGlobalSpokeLineWidth);
        appendLine(lineVertices, right, bottom, color, alpha, kGlobalSpokeLineWidth);
        appendLine(lineVertices, bottom, left, color, alpha, kGlobalSpokeLineWidth);
    } else {
        const QVector3D diagA0 = point + screenSpaceOffsetAtPoint(point, -markerRadius, -markerRadius);
        const QVector3D diagA1 = point + screenSpaceOffsetAtPoint(point, markerRadius, markerRadius);
        const QVector3D diagB0 = point + screenSpaceOffsetAtPoint(point, -markerRadius, markerRadius);
        const QVector3D diagB1 = point + screenSpaceOffsetAtPoint(point, markerRadius, -markerRadius);
        appendLine(lineVertices, diagA0, diagA1, color, alpha, kGlobalSpokeLineWidth);
        appendLine(lineVertices, diagB0, diagB1, color, alpha, kGlobalSpokeLineWidth);
    }
}

void PixelToRayWidget::appendLocalOriginMarker(std::vector<Vertex> &lineVertices,
                                               std::vector<Vertex> &pointVertices,
                                               const QVector3D &origin) const {
    const QColor outerColor(0xff, 0xf4, 0x8a);
    const QColor innerColor(0x0b, 0x12, 0x1d);
    const float outerRadius = 8.0f;
    const float innerRadius = 4.5f;

    const QVector3D left = origin + screenSpaceOffsetAtPoint(origin, -outerRadius, 0.0f);
    const QVector3D right = origin + screenSpaceOffsetAtPoint(origin, outerRadius, 0.0f);
    const QVector3D top = origin + screenSpaceOffsetAtPoint(origin, 0.0f, -outerRadius);
    const QVector3D bottom = origin + screenSpaceOffsetAtPoint(origin, 0.0f, outerRadius);
    appendLine(lineVertices, left, top, outerColor, 0.95f, 1.8f);
    appendLine(lineVertices, top, right, outerColor, 0.95f, 1.8f);
    appendLine(lineVertices, right, bottom, outerColor, 0.95f, 1.8f);
    appendLine(lineVertices, bottom, left, outerColor, 0.95f, 1.8f);

    const QVector3D h0 = origin + screenSpaceOffsetAtPoint(origin, -innerRadius, 0.0f);
    const QVector3D h1 = origin + screenSpaceOffsetAtPoint(origin, innerRadius, 0.0f);
    const QVector3D v0 = origin + screenSpaceOffsetAtPoint(origin, 0.0f, -innerRadius);
    const QVector3D v1 = origin + screenSpaceOffsetAtPoint(origin, 0.0f, innerRadius);
    appendLine(lineVertices, h0, h1, innerColor, 0.98f, 1.6f);
    appendLine(lineVertices, v0, v1, innerColor, 0.98f, 1.6f);

    pointVertices.push_back({origin, QVector4D(1.0f, 0.96f, 0.54f, 1.0f)});
}

bool PixelToRayWidget::loadProjectionImage(const QString &path) {
    QImage image(path);
    if (image.isNull()) {
        return false;
    }
    m_projectionImage = image.convertToFormat(QImage::Format_RGBA8888);
    m_projectionImagePath = QFileInfo(path).absoluteFilePath();
    detectProjectionCircle();
    return true;
}

void PixelToRayWidget::detectProjectionCircle() {
    if (m_projectionImage.isNull()) {
        m_projectionCircleCenter = QPointF();
        m_projectionCircleRadius = 0.0f;
        return;
    }

    const int width = m_projectionImage.width();
    const int height = m_projectionImage.height();
    const int patch = std::max(1, std::min(width, height) / 128);

    auto averagePatch = [&](int x0, int y0) {
        qint64 red = 0;
        qint64 green = 0;
        qint64 blue = 0;
        qint64 count = 0;
        for (int y = y0; y < std::min(height, y0 + patch); ++y) {
            for (int x = x0; x < std::min(width, x0 + patch); ++x) {
                const QColor pixel = QColor::fromRgba(m_projectionImage.pixel(x, y));
                red += pixel.red();
                green += pixel.green();
                blue += pixel.blue();
                ++count;
            }
        }
        return QColor(static_cast<int>(red / std::max<qint64>(1, count)),
                      static_cast<int>(green / std::max<qint64>(1, count)),
                      static_cast<int>(blue / std::max<qint64>(1, count)));
    };

    const QColor backgroundCorners[] = {
        averagePatch(0, 0),
        averagePatch(std::max(0, width - patch), 0),
        averagePatch(0, std::max(0, height - patch)),
        averagePatch(std::max(0, width - patch), std::max(0, height - patch))
    };

    int bgRed = 0;
    int bgGreen = 0;
    int bgBlue = 0;
    for (const QColor &corner : backgroundCorners) {
        bgRed += corner.red();
        bgGreen += corner.green();
        bgBlue += corner.blue();
    }
    const QColor background(bgRed / 4, bgGreen / 4, bgBlue / 4);
    const float thresholdSquared = 28.0f * 28.0f;

    auto isForeground = [&](int x, int y) {
        return colorDistanceSquared(QColor::fromRgba(m_projectionImage.pixel(x, y)), background) > thresholdSquared;
    };

    auto scanLeft = [&](int y) {
        for (int x = 0; x < width; ++x) {
            if (isForeground(x, y)) {
                return x;
            }
        }
        return 0;
    };
    auto scanRight = [&](int y) {
        for (int x = width - 1; x >= 0; --x) {
            if (isForeground(x, y)) {
                return x;
            }
        }
        return width - 1;
    };
    auto scanTop = [&](int x) {
        for (int y = 0; y < height; ++y) {
            if (isForeground(x, y)) {
                return y;
            }
        }
        return 0;
    };
    auto scanBottom = [&](int x) {
        for (int y = height - 1; y >= 0; --y) {
            if (isForeground(x, y)) {
                return y;
            }
        }
        return height - 1;
    };

    const int centerY = height / 2;
    const int centerX = width / 2;
    const int offset = std::max(1, std::min(width, height) / 24);
    const int rowSamples[] = {
        std::clamp(centerY - offset, 0, height - 1),
        centerY,
        std::clamp(centerY + offset, 0, height - 1)
    };
    const int colSamples[] = {
        std::clamp(centerX - offset, 0, width - 1),
        centerX,
        std::clamp(centerX + offset, 0, width - 1)
    };

    int left = 0;
    int right = 0;
    for (int y : rowSamples) {
        left += scanLeft(y);
        right += scanRight(y);
    }
    left /= 3;
    right /= 3;

    int top = 0;
    int bottom = 0;
    for (int x : colSamples) {
        top += scanTop(x);
        bottom += scanBottom(x);
    }
    top /= 3;
    bottom /= 3;

    m_projectionCircleCenter = QPointF(0.5 * static_cast<float>(left + right),
                                       0.5 * static_cast<float>(top + bottom));
    const float radiusX = 0.5f * static_cast<float>(right - left);
    const float radiusY = 0.5f * static_cast<float>(bottom - top);
    m_projectionCircleRadius = 0.5f * (radiusX + radiusY);

    if (m_projectionCircleRadius <= 1.0f) {
        m_projectionCircleCenter = QPointF(0.5 * static_cast<float>(width - 1),
                                           0.5 * static_cast<float>(height - 1));
        m_projectionCircleRadius = 0.5f * static_cast<float>(std::min(width, height));
    }
}

QColor PixelToRayWidget::sampleProjectionColor(const QVector3D &worldPoint) const {
    if (m_projectionImage.isNull()) {
        return QColor();
    }

    const QVector3D globeLocal = m_globeOrientation.conjugated().rotatedVector(worldPoint).normalized();
    const float thetaRadians = std::acos(std::clamp(globeLocal.z(), -1.0f, 1.0f));
    const float thetaDegrees = thetaRadians * 180.0f / kPi;
    if (thetaDegrees > kEntaniyaCapAngleDegrees) {
        return QColor();
    }

    const float xyLength = std::sqrt(globeLocal.x() * globeLocal.x() + globeLocal.y() * globeLocal.y());
    const float capHalfRadians = 0.5f * kEntaniyaCapAngleDegrees * kPi / 180.0f;
    const float radiusNormalized =
        std::sin(0.5f * thetaRadians) / std::sin(capHalfRadians);
    if (radiusNormalized > 1.0f || m_projectionCircleRadius <= 1.0f) {
        return QColor();
    }
    const float dx = xyLength > 1e-6f ? globeLocal.x() / xyLength : 0.0f;
    const float dy = xyLength > 1e-6f ? globeLocal.y() / xyLength : 0.0f;

    const float pixelX = static_cast<float>(m_projectionCircleCenter.x()) + radiusNormalized * m_projectionCircleRadius * dx;
    const float pixelY = static_cast<float>(m_projectionCircleCenter.y()) - radiusNormalized * m_projectionCircleRadius * dy;
    const int baseX = static_cast<int>(std::floor(pixelX));
    const int baseY = static_cast<int>(std::floor(pixelY));
    const float tx = std::clamp(pixelX - static_cast<float>(baseX), 0.0f, 1.0f);
    const float ty = std::clamp(pixelY - static_cast<float>(baseY), 0.0f, 1.0f);

    auto sampleChannel = [&](auto channelGetter) {
        float rowValues[4] = {};
        for (int row = 0; row < 4; ++row) {
            const int y = std::clamp(baseY + row - 1, 0, m_projectionImage.height() - 1);
            float p[4] = {};
            for (int col = 0; col < 4; ++col) {
                const int x = std::clamp(baseX + col - 1, 0, m_projectionImage.width() - 1);
                const QColor sample = QColor::fromRgba(m_projectionImage.pixel(x, y));
                p[col] = static_cast<float>(channelGetter(sample));
            }
            rowValues[row] = catmullRom(p[0], p[1], p[2], p[3], tx);
        }
        return std::clamp(catmullRom(rowValues[0], rowValues[1], rowValues[2], rowValues[3], ty), 0.0f, 255.0f);
    };

    return QColor(static_cast<int>(std::round(sampleChannel([](const QColor &c) { return c.red(); }))),
                  static_cast<int>(std::round(sampleChannel([](const QColor &c) { return c.green(); }))),
                  static_cast<int>(std::round(sampleChannel([](const QColor &c) { return c.blue(); }))));
}

QColor PixelToRayWidget::sampleProjectionColorNearest(const QVector3D &worldPoint) const {
    if (m_projectionImage.isNull()) {
        return QColor();
    }

    const QVector3D globeLocal = m_globeOrientation.conjugated().rotatedVector(worldPoint).normalized();
    const float thetaRadians = std::acos(std::clamp(globeLocal.z(), -1.0f, 1.0f));
    const float thetaDegrees = thetaRadians * 180.0f / kPi;
    if (thetaDegrees > kEntaniyaCapAngleDegrees) {
        return QColor();
    }

    const float xyLength = std::sqrt(globeLocal.x() * globeLocal.x() + globeLocal.y() * globeLocal.y());
    const float capHalfRadians = 0.5f * kEntaniyaCapAngleDegrees * kPi / 180.0f;
    const float radiusNormalized = std::sin(0.5f * thetaRadians) / std::sin(capHalfRadians);
    if (radiusNormalized > 1.0f || m_projectionCircleRadius <= 1.0f) {
        return QColor();
    }

    const float dx = xyLength > 1e-6f ? globeLocal.x() / xyLength : 0.0f;
    const float dy = xyLength > 1e-6f ? globeLocal.y() / xyLength : 0.0f;
    const float pixelX = static_cast<float>(m_projectionCircleCenter.x()) + radiusNormalized * m_projectionCircleRadius * dx;
    const float pixelY = static_cast<float>(m_projectionCircleCenter.y()) - radiusNormalized * m_projectionCircleRadius * dy;
    const int x = std::clamp(static_cast<int>(std::round(pixelX)), 0, m_projectionImage.width() - 1);
    const int y = std::clamp(static_cast<int>(std::round(pixelY)), 0, m_projectionImage.height() - 1);
    return QColor::fromRgba(m_projectionImage.pixel(x, y));
}

void PixelToRayWidget::appendSphereSurface(std::vector<Vertex> &vertices) const {
    const QVector3D center = currentSceneCenter();
    const QVector3D lightDir = QVector3D(0.4f, 0.6f, 1.0f).normalized();
    const int latBands = m_projectionImage.isNull() ? 28 : 256;
    const int lonBands = m_projectionImage.isNull() ? 48 : 512;
    const float minLatitude = minimumVisibleLatitude();

    auto shadedColor = [&](const QVector3D &point) {
        const QColor projectedColor = sampleProjectionColor(point);
        if (projectedColor.isValid()) {
            return projectedColor;
        }
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

    auto shadedCapColor = [&](const QVector3D &point) {
        const QVector3D normal = m_globeOrientation.rotatedVector(QVector3D(0.0f, 0.0f, -1.0f)).normalized();
        const float diffuse = std::max(0.0f, QVector3D::dotProduct(normal, lightDir));
        const float rim = std::pow(1.0f - std::max(0.0f, QVector3D::dotProduct(normal, (cameraPosition() - point).normalized())), 2.0f);
        const float shade = 0.14f + diffuse * 0.24f + rim * 0.16f;
        const QColor base(0x35, 0x2b, 0x20);
        return QColor::fromRgbF(
            std::min(1.0f, base.redF() + shade * 0.26f),
            std::min(1.0f, base.greenF() + shade * 0.20f),
            std::min(1.0f, base.blueF() + shade * 0.12f)
        );
    };

    for (int lat = 0; lat < latBands; ++lat) {
        const float lat0 = minLatitude + (90.0f - minLatitude) * static_cast<float>(lat) / static_cast<float>(latBands);
        const float lat1 = minLatitude + (90.0f - minLatitude) * static_cast<float>(lat + 1) / static_cast<float>(latBands);

        for (int lon = 0; lon < lonBands; ++lon) {
            const float lon0 = 360.0f * static_cast<float>(lon) / static_cast<float>(lonBands);
            const float lon1 = 360.0f * static_cast<float>(lon + 1) / static_cast<float>(lonBands);

            const QVector3D p00 = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, lat0, lon0));
            const QVector3D p01 = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, lat0, lon1));
            const QVector3D p10 = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, lat1, lon0));
            const QVector3D p11 = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, lat1, lon1));

            appendTriangle(vertices, p00, p10, p11, shadedColor((p00 + p10 + p11) / 3.0f), 1.0f);
            appendTriangle(vertices, p00, p11, p01, shadedColor((p00 + p11 + p01) / 3.0f), 1.0f);
        }
    }

    if (minLatitude > -89.9f) {
        const QVector3D capCenterLocal(0.0f, 0.0f, m_globeRadius * std::sin(qDegreesToRadians(minLatitude)));
        const QVector3D capCenter = m_globeOrientation.rotatedVector(capCenterLocal);
        QVector3D previous = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, minLatitude, 0.0f));
        for (int segment = 1; segment <= lonBands; ++segment) {
            const float longitude = 360.0f * static_cast<float>(segment) / static_cast<float>(lonBands);
            const QVector3D current = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, minLatitude, longitude));
            appendTriangle(vertices,
                           capCenter,
                           current,
                           previous,
                           shadedCapColor((capCenter + current + previous) / 3.0f),
                           1.0f);
            previous = current;
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
        QVector3D previous = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, latitude, 0.0f));
        for (int segment = 1; segment <= kSphereSegments; ++segment) {
            const float longitude = (360.0f / static_cast<float>(kSphereSegments)) * static_cast<float>(segment);
            const QVector3D current = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, latitude, longitude));
            const float alpha = alphaForSegment(previous, current);
            if (alpha > 0.0f) {
                appendLine(vertices, previous, current, sphereColor, alpha);
            }
            previous = current;
        }
    }

    for (int meridian = 0; meridian < 8; ++meridian) {
        const float longitude = 45.0f * static_cast<float>(meridian);
        QVector3D previous = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, minLatitude, longitude));
        for (int segment = 1; segment <= kSphereSegments; ++segment) {
            const float latitude = minLatitude
                + (90.0f - minLatitude) * static_cast<float>(segment) / static_cast<float>(kSphereSegments);
            const QVector3D current = m_globeOrientation.rotatedVector(sphericalPoint(m_globeRadius, latitude, longitude));
            const float alpha = alphaForSegment(previous, current);
            if (alpha > 0.0f) {
                appendLine(vertices, previous, current, sphereColor, alpha);
            }
            previous = current;
        }
    }
}

void PixelToRayWidget::appendGlobalAxisTraces(std::vector<Vertex> &lineVertices,
                                              std::vector<Vertex> &positivePointVertices,
                                              std::vector<Vertex> &negativePointVertices,
                                              std::vector<Vertex> &centerPointVertices) const {
    const QVector3D xAxis(1.0f, 0.0f, 0.0f);
    const QVector3D yAxis(0.0f, 1.0f, 0.0f);
    const QVector3D zAxis(0.0f, 0.0f, 1.0f);
    const QVector3D ifnp(0.0f, 0.0f, 0.0f);
    const float minLatitude = minimumVisibleLatitude();

    enum class TraceTexture {
        LongDash,
        DashDot,
        Dotted
    };

    auto appendGreatCircleTrace = [&](const QVector3D &axisA,
                                      const QVector3D &axisB,
                                      const QColor &color,
                                      float alpha,
                                      TraceTexture texture) {
        QVector3D previous;
        bool hasPrevious = false;

        for (int segment = 0; segment <= kPanoramaSegments; ++segment) {
            const float azimuthDeg = 360.0f * static_cast<float>(segment) / static_cast<float>(kPanoramaSegments);
            const float radians = azimuthDeg * kPi / 180.0f;
            const QVector3D point = m_globeOrientation.rotatedVector(
                (axisA * std::cos(radians) + axisB * std::sin(radians)).normalized() * m_globeRadius
            );

            float latitudeDeg = 0.0f;
            float longitudeDeg = 0.0f;
            vectorToLatLon(point, latitudeDeg, longitudeDeg);
            if (latitudeDeg < minLatitude) {
                hasPrevious = false;
                continue;
            }

            if (hasPrevious) {
                bool drawSegment = true;
                switch (texture) {
                case TraceTexture::LongDash:
                    drawSegment = ((segment / 8) % 2) == 0;
                    break;
                case TraceTexture::DashDot: {
                    const int phase = segment % 12;
                    drawSegment = phase < 5 || phase == 7;
                    break;
                }
                case TraceTexture::Dotted:
                    drawSegment = (segment % 4) == 0;
                    break;
                }

                if (drawSegment) {
                    appendLine(lineVertices, previous, point, color, alpha);
                }
            }
            previous = point;
            hasPrevious = true;
        }
    };

    auto appendDashedSpoke = [&](const QVector3D &endPoint,
                                 const QColor &color,
                                 float alpha) {
        constexpr int kDashSegments = 12;
        for (int segment = 0; segment < kDashSegments; ++segment) {
            if ((segment % 2) != 0) {
                continue;
            }
            const float t0 = static_cast<float>(segment) / static_cast<float>(kDashSegments);
            const float t1 = static_cast<float>(segment + 1) / static_cast<float>(kDashSegments);
            const QVector3D p0 = ifnp + (endPoint - ifnp) * t0;
            const QVector3D p1 = ifnp + (endPoint - ifnp) * t1;
            appendLine(lineVertices, p0, p1, color, alpha, kGlobalSpokeLineWidth);
        }
    };

    // Globe-axis naming:
    // i_r_a = IdealRowAxis = global X axis
    // i_o_a = IdealOpticalAxis = global Y axis
    // i_c_a = IdealColumnAxis = global Z axis
    const QColor globalXColor = globalTraceColor('x');
    const QColor globalYColor = globalTraceColor('y');
    const QColor globalZColor = globalTraceColor('z');

    appendGreatCircleTrace(yAxis, zAxis, globalXColor, 0.74f, TraceTexture::LongDash);
    appendGreatCircleTrace(zAxis, xAxis, globalYColor, 0.76f, TraceTexture::DashDot);
    appendGreatCircleTrace(xAxis, yAxis, globalZColor, 0.80f, TraceTexture::Dotted);

    const QVector3D positiveXEnd = m_globeOrientation.rotatedVector(ifnp + xAxis * m_globeRadius);
    const QVector3D negativeXEnd = m_globeOrientation.rotatedVector(ifnp - xAxis * m_globeRadius);
    const QVector3D positiveYEnd = m_globeOrientation.rotatedVector(ifnp + yAxis * m_globeRadius);
    const QVector3D negativeYEnd = m_globeOrientation.rotatedVector(ifnp - yAxis * m_globeRadius);
    const QVector3D positiveZEnd = m_globeOrientation.rotatedVector(ifnp + zAxis * m_globeRadius);
    const QVector3D negativeZEnd = m_globeOrientation.rotatedVector(ifnp - zAxis * m_globeRadius);

    // Global-axis naming:
    // i_r_a = IdealRowAxis = global X axis
    // i_o_a = IdealOpticalAxis = global Y axis
    // i_c_a = IdealColumnAxis = global Z axis
    appendDashedSpoke(positiveXEnd, globalXColor, 0.98f);
    appendDashedSpoke(negativeXEnd, globalXColor, 0.68f);
    appendDashedSpoke(positiveYEnd, globalYColor, 0.98f);
    appendDashedSpoke(negativeYEnd, globalYColor, 0.68f);
    appendDashedSpoke(positiveZEnd, globalZColor, 0.98f);
    appendDashedSpoke(negativeZEnd, globalZColor, 0.72f);

    appendGlobalTerminalMarker(lineVertices, positiveXEnd, globalXColor, true);
    appendGlobalTerminalMarker(lineVertices, negativeXEnd, globalXColor, false);
    appendGlobalTerminalMarker(lineVertices, positiveYEnd, globalYColor, true);
    appendGlobalTerminalMarker(lineVertices, negativeYEnd, globalYColor, false);
    appendGlobalTerminalMarker(lineVertices, positiveZEnd, globalZColor, true);
    appendGlobalTerminalMarker(lineVertices, negativeZEnd, globalZColor, false);

    positivePointVertices.push_back({positiveXEnd, QVector4D(globalXColor.redF(), globalXColor.greenF(), globalXColor.blueF(), 1.0f)});
    negativePointVertices.push_back({negativeXEnd, QVector4D(globalXColor.redF(), globalXColor.greenF(), globalXColor.blueF(), 0.88f)});
    positivePointVertices.push_back({positiveYEnd, QVector4D(globalYColor.redF(), globalYColor.greenF(), globalYColor.blueF(), 1.0f)});
    negativePointVertices.push_back({negativeYEnd, QVector4D(globalYColor.redF(), globalYColor.greenF(), globalYColor.blueF(), 0.88f)});
    positivePointVertices.push_back({positiveZEnd, QVector4D(globalZColor.redF(), globalZColor.greenF(), globalZColor.blueF(), 1.0f)});
    negativePointVertices.push_back({negativeZEnd, QVector4D(globalZColor.redF(), globalZColor.greenF(), globalZColor.blueF(), 0.88f)});
    centerPointVertices.push_back({ifnp, QVector4D(1.0f, 1.0f, 1.0f, 1.0f)});
}

void PixelToRayWidget::appendPanoramaBand(std::vector<Vertex> &cylinderSurfaceVertices,
                                          std::vector<Vertex> &lineVertices) const {
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
            m_globeRadius + m_components[static_cast<int>(FrameMath::ComponentId::PositiveZ)];
        const float positiveY = m_components[static_cast<int>(FrameMath::ComponentId::PositiveY)];
        const float negativeY = m_components[static_cast<int>(FrameMath::ComponentId::NegtiveY)];
        //const float positiveHeight = std::sqrt(xCylinderRadius * xCylinderRadius + positiveY * positiveY);
        //const float negativeHeight = std::sqrt(xCylinderRadius * xCylinderRadius + negativeY * negativeY);
        const float positiveHeight =  positiveY * xCylinderRadius;
        const float negativeHeight = negativeY  * xCylinderRadius;

        const QVector3D camera = cameraPosition();
        for (int segment = 0; segment < kPanoramaSegments; ++segment) {
            const float azimuth0 = 360.0f * static_cast<float>(segment) / static_cast<float>(kPanoramaSegments);
            const float azimuth1 = 360.0f * static_cast<float>(segment + 1) / static_cast<float>(kPanoramaSegments);

            const QVector3D base0 = circlePoint(frame.zAxis, frame.xAxis, azimuth0, xCylinderRadius);
            const QVector3D base1 = circlePoint(frame.zAxis, frame.xAxis, azimuth1, xCylinderRadius);
            const QVector3D top0 = base0 + frame.yAxis * positiveHeight;
            const QVector3D top1 = base1 + frame.yAxis * positiveHeight;
            const QVector3D bottom0 = base0 - frame.yAxis * negativeHeight;
            const QVector3D bottom1 = base1 - frame.yAxis * negativeHeight;
            const QVector3D radialMid = ((base0 + base1) * 0.5f).normalized();
            const QVector3D segmentMid = (base0 + base1 + top0 + top1) * 0.25f;
            const bool isFrontFacing = QVector3D::dotProduct(radialMid, (camera - segmentMid).normalized()) >= 0.0f;
            const bool xrayCylinder = !m_state.hideSurfaceDepth;
            const float positiveFillAlpha = 1.0f;
            const float negativeFillAlpha = 1.0f;
            const float traceAlpha = isFrontFacing ? 0.78f : (xrayCylinder ? 0.48f : 0.30f);
            const float hatchAlpha = xrayCylinder ? 0.42f : 0.24f;

            const QColor positiveSampleA = sampleProjectionColorNearest((base0 + base1 + top1) / 3.0f);
            const QColor positiveSampleB = sampleProjectionColorNearest((base0 + top1 + top0) / 3.0f);
            const QColor negativeSampleA = sampleProjectionColorNearest((bottom0 + bottom1 + base1) / 3.0f);
            const QColor negativeSampleB = sampleProjectionColorNearest((bottom0 + base1 + base0) / 3.0f);

            appendTriangle(cylinderSurfaceVertices,
                           base0,
                           base1,
                           top1,
                           positiveSampleA.isValid() ? positiveSampleA : xPositiveCylinderFill,
                           positiveFillAlpha);
            appendTriangle(cylinderSurfaceVertices,
                           base0,
                           top1,
                           top0,
                           positiveSampleB.isValid() ? positiveSampleB : xPositiveCylinderFill,
                           positiveFillAlpha);
            appendTriangle(cylinderSurfaceVertices,
                           bottom0,
                           bottom1,
                           base1,
                           negativeSampleA.isValid() ? negativeSampleA : xNegativeCylinderFill,
                           negativeFillAlpha);
            appendTriangle(cylinderSurfaceVertices,
                           bottom0,
                           base1,
                           base0,
                           negativeSampleB.isValid() ? negativeSampleB : xNegativeCylinderFill,
                           negativeFillAlpha);
            appendLine(lineVertices, top0, top1, xPositiveTraceColor, traceAlpha);
            appendLine(lineVertices, bottom0, bottom1, xNegativeTraceColor, traceAlpha);

            if (!isFrontFacing && (segment % 2) == 0) {
                appendLine(lineVertices, base0, top1, xPositiveTraceColor, hatchAlpha);
                appendLine(lineVertices, bottom0, base1, xNegativeTraceColor, hatchAlpha);
            }
        }

        appendGreatCircleTrace(frame.zAxis, frame.xAxis, xPositiveTraceColor, xCylinderRadius);
    }

    if (m_state.showPanoramaBand) {
        appendGreatCircleTrace(frame.zAxis, frame.yAxis, yTraceColor, m_globeRadius);
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
        appendLocalOriginMarker(lineVertices, pointVertices, origin);
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
        const bool isActive = m_hasActiveComponent && id == m_state.active;
        const bool isStaticXYGuide = meta.axis != 'z' && !isActive;
        const float guideAlpha = isActive ? 1.0f : (isStaticXYGuide ? 0.28f : 0.65f);
        appendLine(lineVertices, origin, origin + direction * m_components[index], color, guideAlpha);
        pointVertices.push_back({origin + direction * m_components[index], QVector4D(color.redF(), color.greenF(), color.blueF(), 1.0f)});

        if (isActive) {
            appendLine(lineVertices, origin, origin + direction * m_components[index], QColor(0x0b, 0x12, 0x1d), 1.0f);
            appendLine(activeLineVertices,
                       origin,
                       origin + direction * m_components[index],
                       QColor(0xff, 0xf4, 0x8a),
                       0.95f,
                       kActiveContourLineWidth);
        }
    }

    appendLocalOriginMarker(lineVertices, pointVertices, origin);
}

void PixelToRayWidget::drawVertices(GLenum mode,
                                    const std::vector<Vertex> &vertices,
                                    float lineWidth,
                                    float pointSize,
                                    bool depthWrite,
                                    bool depthTest) {
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

    if (depthTest) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    if (mode != GL_TRIANGLES) {
        glLineWidth(lineWidth);
    }

    glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
    glDrawArrays(mode, 0, static_cast<GLsizei>(vertices.size()));
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
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
    return tryMapScreenToSphere(position, false, true, spherePoint);
}

bool PixelToRayWidget::tryMapScreenToSphere(const QPointF &position, bool preferRearHemisphere, QVector3D &spherePoint) const {
    return tryMapScreenToSphere(position, preferRearHemisphere, true, spherePoint);
}

bool PixelToRayWidget::tryMapScreenToSphere(const QPointF &position,
                                            bool preferRearHemisphere,
                                            bool requireVisibleCap,
                                            QVector3D &spherePoint) const {
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
    const float c = QVector3D::dotProduct(nearPoint, nearPoint) - m_globeRadius * m_globeRadius;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return false;
    }

    const float root = std::sqrt(discriminant);
    const float t0 = (-b - root) / (2.0f * a);
    const float t1 = (-b + root) / (2.0f * a);
    const float nearT = std::min(t0, t1);
    const float farT = std::max(t0, t1);
    float t = preferRearHemisphere ? farT : nearT;
    if (t < 0.0f) {
        t = preferRearHemisphere ? nearT : farT;
    }
    if (t < 0.0f) {
        return false;
    }

    spherePoint = nearPoint + rayDirection * t;
    if (!requireVisibleCap) {
        return true;
    }

    const QVector3D globeLocalPoint = m_globeOrientation.conjugated().rotatedVector(spherePoint);
    float latitudeDeg = 0.0f;
    float longitudeDeg = 0.0f;
    vectorToLatLon(globeLocalPoint, latitudeDeg, longitudeDeg);
    if (latitudeDeg < minimumVisibleLatitude()) {
        return false;
    }
    return true;
}

bool PixelToRayWidget::tryPickGlobalAxisEndpoint(const QPointF &position,
                                                 QVector3D &pickedWorldPoint,
                                                 bool &preferRearHemisphere) const {
    const_cast<PixelToRayWidget *>(this)->updateViewProjection();
    const QVector3D center = currentSceneCenter();
    const QVector3D cameraDir = (cameraPosition() - center).normalized();

    struct EndpointCandidate {
        QPointF screen;
        QVector3D worldPoint;
        bool preferRear = false;
    };

    const QVector3D endpoints[] = {
        m_globeOrientation.rotatedVector(QVector3D(m_globeRadius, 0.0f, 0.0f)),
        m_globeOrientation.rotatedVector(QVector3D(-m_globeRadius, 0.0f, 0.0f)),
        m_globeOrientation.rotatedVector(QVector3D(0.0f, m_globeRadius, 0.0f)),
        m_globeOrientation.rotatedVector(QVector3D(0.0f, -m_globeRadius, 0.0f)),
        m_globeOrientation.rotatedVector(QVector3D(0.0f, 0.0f, m_globeRadius)),
        m_globeOrientation.rotatedVector(QVector3D(0.0f, 0.0f, -m_globeRadius)),
    };

    std::optional<EndpointCandidate> best;
    float bestDistanceSquared = kGlobalAxisPickRadius * kGlobalAxisPickRadius;
    for (const QVector3D &endpoint : endpoints) {
        const QPointF screen = projectPoint(endpoint).screen;
        const QPointF delta = screen - position;
        const float distanceSquared =
            static_cast<float>(delta.x() * delta.x() + delta.y() * delta.y());
        if (distanceSquared > bestDistanceSquared) {
            continue;
        }

        const bool isRear = QVector3D::dotProduct(cameraDir, endpoint - center) < 0.0f;
        best = EndpointCandidate{screen, endpoint, isRear};
        bestDistanceSquared = distanceSquared;
    }

    if (!best.has_value()) {
        return false;
    }
    pickedWorldPoint = best->worldPoint;
    preferRearHemisphere = best->preferRear;
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
    m_components[static_cast<int>(FrameMath::ComponentId::NegtiveZ)] = m_globeRadius;
}

void PixelToRayWidget::updateSurfaceTracking(const QPointF &position, bool preferRearHemisphere) {
    QVector3D spherePoint;
    if (!tryMapScreenToSphere(position, preferRearHemisphere, spherePoint)) {
        return;
    }

    const QVector3D globeLocalPoint = m_globeOrientation.conjugated().rotatedVector(spherePoint);
    float latitude = m_state.latitude;
    float longitude = m_state.longitude;
    vectorToLatLon(globeLocalPoint, latitude, longitude);
    if (!m_locks.latitude) {
        m_state.latitude = latitude;
    }
    if (!m_locks.longitude) {
        m_state.longitude = longitude;
    }
    clampStateToVisibleCap();
}

void PixelToRayWidget::updateGlobalAxisTracking(const QPointF &position) {
    QVector3D spherePoint;
    if (!tryMapScreenToSphere(position, m_drag.preferRearHemisphere, false, spherePoint)) {
        return;
    }

    const QVector3D targetDirection = spherePoint.normalized();
    const QVector3D startDirection = m_drag.grabbedWorldPoint.normalized();
    if (startDirection.lengthSquared() < 1e-6f || targetDirection.lengthSquared() < 1e-6f) {
        return;
    }

    const QQuaternion delta = QQuaternion::rotationTo(startDirection, targetDirection);
    m_globeOrientation = (delta * m_drag.startOrientation).normalized();
}

void PixelToRayWidget::emitStatus() {
    m_state.active = normalizedActiveComponent(m_state.active);
    m_components[static_cast<int>(FrameMath::ComponentId::NegtiveZ)] = m_globeRadius;
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

    QAction *panoramaAction = menu.addAction(tr("y-axis great-circle trace"));
    panoramaAction->setCheckable(true);
    panoramaAction->setChecked(m_state.showPanoramaBand);

    QAction *xCylinderAction = menu.addAction(tr("x-axis tangential cylinder"));
    xCylinderAction->setCheckable(true);
    xCylinderAction->setChecked(m_state.showXTangentCylinder);

    QAction *depthHidingAction = menu.addAction(tr("Surface depth hiding"));
    depthHidingAction->setCheckable(true);
    depthHidingAction->setChecked(m_state.hideSurfaceDepth);

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
    } else if (chosen == depthHidingAction) {
        m_state.hideSurfaceDepth = depthHidingAction->isChecked();
    } else if (chosen == centerAction) {
        m_state.fixGlobalCenter = centerAction->isChecked();
    }

    emitStatus();
    update();
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

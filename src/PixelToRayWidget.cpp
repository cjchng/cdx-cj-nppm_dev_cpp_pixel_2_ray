#include "PixelToRayWidget.h"

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
constexpr float kHandleRadius = 8.0f;
constexpr float kHandlePointSize = 14.0f;
constexpr float kActiveHandlePointSize = 20.0f;

QString statusText(const FrameMath::State &state, const FrameMath::ComponentLengths &components, float zoom) {
    using FrameMath::componentKey;

    auto componentValue = [&](FrameMath::ComponentId id) {
        return QStringLiteral("%1: %2")
            .arg(componentKey(id))
            .arg(components[static_cast<int>(id)], 0, 'f', 2);
    };

    return QStringLiteral(
               "Qt rewrite: left-drag handle length\n"
               "active: %1\n"
               "lat: %2\n"
               "lon: %3\n"
               "roll: %4\n"
               "zoom: %5\n"
               "center: %6\n"
               "\n"
               "%7\n"
               "%8\n"
               "%9\n"
               "%10\n"
               "%11\n"
               "%12\n"
               "\n"
               "x/X y/Y z/Z    : select +/- component\n"
               "w/s            : latitude +/-\n"
               "a/d            : longitude -/+\n"
               "q/e            : roll -/+\n"
               "wheel          : zoom\n"
               "right click    : context menu")
        .arg(FrameMath::componentKey(state.active))
        .arg(state.latitude, 0, 'f', 1)
        .arg(state.longitude, 0, 'f', 1)
        .arg(state.roll, 0, 'f', 1)
        .arg(zoom, 0, 'f', 2)
        .arg(state.fixGlobalCenter ? QStringLiteral("fixed") : QStringLiteral("move"))
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
        0.3f
    };
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

void PixelToRayWidget::resizeGL(int, int) {
    updateViewProjection();
    emitStatus();
}

void PixelToRayWidget::paintGL() {
    updateViewProjection();
    glViewport(0, 0, width(), height());
    glClearColor(0.06f, 0.10f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::vector<Vertex> surfaceVertices;
    std::vector<Vertex> lineVertices;
    std::vector<Vertex> pointVertices;
    std::vector<Vertex> activePointVertices;
    surfaceVertices.reserve(12000);
    lineVertices.reserve(4000);
    pointVertices.reserve(16);
    activePointVertices.reserve(2);

    appendSphereSurface(surfaceVertices);
    appendSphereWireframe(lineVertices);
    appendFrameGeometry(lineVertices, pointVertices);

    const QVector3D activeTip =
        currentOrigin() + FrameMath::componentDirection(currentFrame(), m_state.active) * m_components[static_cast<int>(m_state.active)];
    activePointVertices.push_back({activeTip, QVector4D(1.0f, 0.96f, 0.54f, 1.0f)});

    drawVertices(GL_TRIANGLES, surfaceVertices, 1.0f, 1.0f);
    drawVertices(GL_LINES, lineVertices, 1.8f, 1.0f);
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
    if (!tryPickHandle(event->position(), picked)) {
        event->ignore();
        return;
    }

    m_state.active = picked;
    m_drag.active = true;
    m_drag.component = picked;
    m_drag.startLength = m_components[static_cast<int>(picked)];
    m_drag.startMouse = event->position();

    emitStatus();
    update();
}

void PixelToRayWidget::mouseMoveEvent(QMouseEvent *event) {
    if (!m_drag.active) {
        return;
    }

    updateDrag(event->position());
    emitStatus();
    update();
}

void PixelToRayWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_drag.active) {
        m_drag.active = false;
        emitStatus();
        update();
    }
}

void PixelToRayWidget::wheelEvent(QWheelEvent *event) {
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
                             ? FrameMath::ComponentId::NegtiveZ
                             : FrameMath::ComponentId::PositiveZ;
        break;
    case Qt::Key_W:
        m_state.latitude = std::min(89.0f, m_state.latitude + 2.0f);
        break;
    case Qt::Key_S:
        m_state.latitude = std::max(-89.0f, m_state.latitude - 2.0f);
        break;
    case Qt::Key_A:
        m_state.longitude -= 2.0f;
        break;
    case Qt::Key_D:
        m_state.longitude += 2.0f;
        break;
    case Qt::Key_Q:
        m_state.roll -= 3.0f;
        break;
    case Qt::Key_E:
        m_state.roll += 3.0f;
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
        return;
    }

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
}

void PixelToRayWidget::ensureGlResources() {
    if (m_program) {
        return;
    }

    m_program.reset(new QOpenGLShaderProgram());
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
        R"(#version 330 core
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec4 aColor;
        uniform mat4 uMvp;
        uniform float uPointSize;
        out vec4 vColor;
        void main() {
            gl_Position = uMvp * vec4(aPosition, 1.0);
            gl_PointSize = uPointSize;
            vColor = aColor;
        })");
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
        R"(#version 330 core
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
        })");
    m_program->link();

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

void PixelToRayWidget::appendLine(std::vector<Vertex> &vertices, const QVector3D &a, const QVector3D &b, const QColor &color, float alpha) const {
    const QVector4D rgba(color.redF(), color.greenF(), color.blueF(), alpha);
    vertices.push_back({a, rgba});
    vertices.push_back({b, rgba});
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

    auto shadedColor = [&](const QVector3D &point) {
        const QVector3D normal = (point - center).normalized();
        const float diffuse = std::max(0.0f, QVector3D::dotProduct(normal, lightDir));
        const float rim = std::pow(1.0f - std::max(0.0f, QVector3D::dotProduct(normal, (cameraPosition() - point).normalized())), 2.0f);
        const float shade = 0.18f + diffuse * 0.30f + rim * 0.22f;
        const QColor base(0x2a, 0x3d, 0x5f);
        return QColor::fromRgbF(
            std::min(1.0f, base.redF() + shade * 0.25f),
            std::min(1.0f, base.greenF() + shade * 0.30f),
            std::min(1.0f, base.blueF() + shade * 0.38f)
        );
    };

    for (int lat = 0; lat < latBands; ++lat) {
        const float lat0 = -90.0f + 180.0f * static_cast<float>(lat) / static_cast<float>(latBands);
        const float lat1 = -90.0f + 180.0f * static_cast<float>(lat + 1) / static_cast<float>(latBands);

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

    auto alphaForSegment = [&](const QVector3D &a, const QVector3D &b) {
        const bool aFront = QVector3D::dotProduct(cameraDir, a - center) >= 0.0f;
        const bool bFront = QVector3D::dotProduct(cameraDir, b - center) >= 0.0f;
        if (aFront && bFront) {
            return 0.70f;
        }
        return m_state.showRearWireframe ? 0.22f : -1.0f;
    };

    const QColor sphereColor(0xe5, 0xe7, 0xeb);
    for (int ring = -kGridSteps; ring <= kGridSteps; ++ring) {
        const float latitude = (80.0f / static_cast<float>(kGridSteps)) * static_cast<float>(ring);
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
        QVector3D previous = sphericalPoint(FrameMath::kSphereRadius, -90.0f, longitude);
        for (int segment = 1; segment <= kSphereSegments; ++segment) {
            const float latitude = -90.0f + (180.0f / static_cast<float>(kSphereSegments)) * static_cast<float>(segment);
            const QVector3D current = sphericalPoint(FrameMath::kSphereRadius, latitude, longitude);
            const float alpha = alphaForSegment(previous, current);
            if (alpha > 0.0f) {
                appendLine(vertices, previous, current, sphereColor, alpha);
            }
            previous = current;
        }
    }
}

void PixelToRayWidget::appendFrameGeometry(std::vector<Vertex> &lineVertices, std::vector<Vertex> &pointVertices) const {
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
        const QColor color = FrameMath::axisColor(FrameMath::componentMeta(id).axis);
        appendLine(lineVertices, origin, origin + direction * m_components[index], color, id == m_state.active ? 1.0f : 0.65f);
        pointVertices.push_back({origin + direction * m_components[index], QVector4D(color.redF(), color.greenF(), color.blueF(), 1.0f)});

        if (id == m_state.active) {
            appendLine(lineVertices, origin, origin + direction * m_components[index], QColor(0x0b, 0x12, 0x1d), 1.0f);
            appendLine(lineVertices, origin, origin + direction * m_components[index], QColor(0xff, 0xf4, 0x8a), 0.95f);
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
    glLineWidth(lineWidth);
    if (mode == GL_TRIANGLES) {
        glDepthMask(GL_FALSE);
    } else {
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

std::vector<InteractionLogic::HandleCandidate> PixelToRayWidget::handleCandidates() const {
    std::vector<InteractionLogic::HandleCandidate> candidates;
    candidates.reserve(static_cast<int>(FrameMath::ComponentId::Count));

    for (int index = 0; index < static_cast<int>(FrameMath::ComponentId::Count); ++index) {
        const auto id = static_cast<FrameMath::ComponentId>(index);
        const HandleVisual handle = handleVisual(id);
        candidates.push_back({id, handle.center, handle.radius, handle.visible});
    }

    return candidates;
}

void PixelToRayWidget::updateDrag(const QPointF &position) {
    updateViewProjection();
    const QVector3D origin = currentOrigin();
    const QVector3D direction = FrameMath::componentDirection(currentFrame(), m_drag.component);
    const QVector2D axisScreen = projectVectorToScreen(origin, direction, 1.0f);
    const float deltaLength = InteractionLogic::lengthDeltaFromScreenDrag(m_drag.startMouse, position, axisScreen);
    m_components[static_cast<int>(m_drag.component)] = FrameMath::clampLength(m_drag.startLength + deltaLength);
}

void PixelToRayWidget::emitStatus() {
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

    QAction *centerAction = menu.addAction(tr("Global center (fix/move)"));
    centerAction->setCheckable(true);
    centerAction->setChecked(m_state.fixGlobalCenter);

    QAction *chosen = menu.exec(globalPosition);
    if (chosen == rearAction) {
        m_state.showRearWireframe = rearAction->isChecked();
    } else if (chosen == frameAction) {
        m_state.showLocalFrame = frameAction->isChecked();
    } else if (chosen == centerAction) {
        m_state.fixGlobalCenter = centerAction->isChecked();
    }

    emitStatus();
    update();
}

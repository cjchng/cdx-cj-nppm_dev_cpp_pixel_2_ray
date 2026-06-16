#pragma once

#include "FrameMath.h"
#include "InteractionLogic.h"

#include <QMenu>
#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPointF>
#include <QQuaternion>
#include <QScopedPointer>
#include <QImage>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <vector>

class PixelToRayWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PixelToRayWidget(QWidget *parent = nullptr);
    ~PixelToRayWidget() override;
    float latitude() const;
    float longitude() const;
    float roll() const;
    float zoom() const;
    float capPolarAngle() const;
    float globeRadius() const;
    QQuaternion globeOrientation() const;
    QString fisheyeImagePath() const;
    float perspectiveFovY() const;
    float cameraEyeOffsetX() const;
    float cameraEyeOffsetY() const;
    float cameraEyeOffsetZ() const;
    float cameraTargetX() const;
    float cameraTargetY() const;
    float cameraTargetZ() const;
    float cameraUpX() const;
    float cameraUpY() const;
    float cameraUpZ() const;
    float anchorGlobalX() const;
    float anchorGlobalY() const;
    float anchorGlobalZ() const;
    float componentLength(FrameMath::ComponentId id) const;
    QString activeComponentKey() const;
    bool isRearHemisphereWireframeVisible() const;
    bool isWorldAxesVisible() const;
    bool isLocalCoordinateFrameVisible() const;
    bool isGlobalCenterFixed() const;
    bool isPanoramaBandVisible() const;
    bool isXTangentCylinderVisible() const;
    bool isSurfaceDepthHidingEnabled() const;
    bool isCapClosed() const;
    bool isPerspectiveProjection() const;
    bool isLatitudeLocked() const;
    bool isLongitudeLocked() const;
    bool isRollLocked() const;
    bool isZoomLocked() const;
    bool isCapPolarAngleLocked() const;
    bool isGlobeRadiusLocked() const;
    bool isCameraEyeOffsetXLocked() const;
    bool isCameraEyeOffsetYLocked() const;
    bool isCameraEyeOffsetZLocked() const;
    bool isCameraUpXLocked() const;
    bool isCameraUpYLocked() const;
    bool isCameraUpZLocked() const;
    bool isGlobeOrientationScalarLocked() const;
    bool isGlobeOrientationXLocked() const;
    bool isGlobeOrientationYLocked() const;
    bool isGlobeOrientationZLocked() const;
    bool isComponentLocked(FrameMath::ComponentId id) const;
    void setLatitude(float value);
    void setLongitude(float value);
    void setRoll(float value);
    void setZoom(float value);
    void setCapPolarAngle(float value);
    void setGlobeRadius(float value);
    void setGlobeOrientation(const QQuaternion &orientation);
    bool setFisheyeImagePath(const QString &path);
    void setPerspectiveFovY(float value);
    void setCameraEyeOffsetX(float value);
    void setCameraEyeOffsetY(float value);
    void setCameraEyeOffsetZ(float value);
    void setCameraUpX(float value);
    void setCameraUpY(float value);
    void setCameraUpZ(float value);
    void setGlobeRadiusLocked(bool locked);
    void setComponentLength(FrameMath::ComponentId id, float value);
    void setRearHemisphereWireframeVisible(bool visible);
    void setWorldAxesVisible(bool visible);
    void setLocalCoordinateFrameVisible(bool visible);
    void setGlobalCenterFixed(bool fixed);
    void setPanoramaBandVisible(bool visible);
    void setXTangentCylinderVisible(bool visible);
    void setSurfaceDepthHidingEnabled(bool enabled);
    void setCapClosed(bool closed);
    void setPerspectiveProjection(bool enabled);
    void setLatitudeLocked(bool locked);
    void setLongitudeLocked(bool locked);
    void setRollLocked(bool locked);
    void setZoomLocked(bool locked);
    void setCapPolarAngleLocked(bool locked);
    void setCameraEyeOffsetXLocked(bool locked);
    void setCameraEyeOffsetYLocked(bool locked);
    void setCameraEyeOffsetZLocked(bool locked);
    void setCameraUpXLocked(bool locked);
    void setCameraUpYLocked(bool locked);
    void setCameraUpZLocked(bool locked);
    void setGlobeOrientationScalarLocked(bool locked);
    void setGlobeOrientationXLocked(bool locked);
    void setGlobeOrientationYLocked(bool locked);
    void setGlobeOrientationZLocked(bool locked);
    void setComponentLocked(FrameMath::ComponentId id, bool locked);

signals:
    void statusTextChanged(const QString &text);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct Vertex {
        QVector3D position;
        QVector4D color;
    };

    struct TexturedCylinderVertex {
        QVector3D position;
        QVector4D fallbackColor;
    };

    struct ProjectedPoint {
        QPointF screen;
        float depth = 0.0f;
    };

    struct DragState {
        enum class Mode {
            None,
            ComponentLength,
            SurfaceTrack,
            GlobalAxisTrack
        };

        bool active = false;
        Mode mode = Mode::None;
        FrameMath::ComponentId component = FrameMath::ComponentId::PositiveX;
        float startLength = 0.0f;
        QPointF startMouse;
        bool preferRearHemisphere = false;
        QQuaternion startOrientation;
        QVector3D grabbedWorldPoint;
    };

    struct HandleVisual {
        QPointF center;
        float radius = 0.0f;
        bool visible = false;
    };

    struct LockState {
        bool latitude = false;
        bool longitude = false;
        bool roll = false;
        bool zoom = false;
        bool capPolarAngle = false;
        bool globeRadius = true;
        bool cameraEyeOffsetX = false;
        bool cameraEyeOffsetY = false;
        bool cameraEyeOffsetZ = false;
        bool cameraUpX = false;
        bool cameraUpY = false;
        bool cameraUpZ = false;
        bool globeOrientationScalar = false;
        bool globeOrientationX = false;
        bool globeOrientationY = false;
        bool globeOrientationZ = false;
        FrameMath::ComponentLengths components{false, false, false, false, false, true};
    };

    FrameMath::Frame currentFrame() const;
    QVector3D currentOrigin() const;
    QVector3D currentSceneCenter() const;
    QVector3D cameraTarget() const;
    QVector3D cameraPosition() const;
    QVector3D cameraUp() const;
    QVector3D cameraUpForView() const;
    float effectiveCapPolarAngle() const;
    float minimumVisibleLatitude() const;
    void clampStateToVisibleCap();
    float pixelsPerUnit() const;
    bool projectedSurfaceBoundsCenterNdc(QVector2D &centerNdc) const;
    void updateViewProjection();
    void ensureGlResources();
    void refreshProjectionTexture();
    void appendLine(std::vector<Vertex> &vertices,
                    const QVector3D &a,
                    const QVector3D &b,
                    const QColor &color,
                    float alpha = 1.0f,
                    float widthPixels = 0.0f) const;
    void appendTriangle(std::vector<Vertex> &vertices, const QVector3D &a, const QVector3D &b, const QVector3D &c, const QColor &color, float alpha = 1.0f) const;
    void appendTexturedCylinderTriangle(std::vector<TexturedCylinderVertex> &vertices,
                                        const QVector3D &a,
                                        const QVector3D &b,
                                        const QVector3D &c,
                                        const QColor &fallbackColor,
                                        float alpha = 1.0f) const;
    QVector3D screenSpaceOffsetAtPoint(const QVector3D &point, float dxPixels, float dyPixels) const;
    void appendGlobalTerminalMarker(std::vector<Vertex> &lineVertices,
                                    const QVector3D &point,
                                    const QColor &color,
                                    bool positive) const;
    void appendLocalOriginMarker(std::vector<Vertex> &lineVertices,
                                 std::vector<Vertex> &pointVertices,
                                 const QVector3D &origin) const;
    void appendSphereSurface(std::vector<Vertex> &vertices) const;
    void appendSphereWireframe(std::vector<Vertex> &vertices) const;
    bool loadProjectionImage(const QString &path);
    void detectProjectionCircle();
    QColor sampleProjectionColorNearest(const QVector3D &worldPoint) const;
    QColor sampleProjectionColor(const QVector3D &worldPoint) const;
    void appendGlobalAxisTraces(std::vector<Vertex> &lineVertices,
                                std::vector<Vertex> &positivePointVertices,
                                std::vector<Vertex> &negativePointVertices,
                                std::vector<Vertex> &centerPointVertices) const;
    void appendXTangentCylinder(std::vector<Vertex> &cylinderSurfaceVertices,
                                std::vector<TexturedCylinderVertex> &texturedCylinderSurfaceVertices,
                                std::vector<Vertex> &lineVertices,
                                const FrameMath::Frame &frame) const;
    void appendPanoramaBand(std::vector<Vertex> &cylinderSurfaceVertices,
                            std::vector<TexturedCylinderVertex> &texturedCylinderSurfaceVertices,
                            std::vector<Vertex> &lineVertices) const;
    void appendFrameGeometry(std::vector<Vertex> &lineVertices,
                             std::vector<Vertex> &activeLineVertices,
                             std::vector<Vertex> &pointVertices) const;
    void drawVertices(GLenum mode,
                      const std::vector<Vertex> &vertices,
                      float lineWidth = 1.0f,
                      float pointSize = 1.0f,
                      bool depthWrite = true,
                      bool depthTest = true);
    void drawTexturedCylinderVertices(const std::vector<TexturedCylinderVertex> &vertices,
                                      bool depthWrite,
                                      bool depthTest);
    ProjectedPoint projectPoint(const QVector3D &worldPoint) const;
    QVector2D projectVectorToScreen(const QVector3D &origin, const QVector3D &direction, float length = 1.0f) const;
    HandleVisual handleVisual(FrameMath::ComponentId id) const;
    std::vector<InteractionLogic::HandleCandidate> handleCandidates() const;
    bool tryPickHandle(const QPointF &position, FrameMath::ComponentId &picked) const;
    bool tryPickGlobalAxisEndpoint(const QPointF &position,
                                   QVector3D &pickedWorldPoint,
                                   bool &preferRearHemisphere) const;
    bool tryMapScreenToSphere(const QPointF &position, QVector3D &spherePoint) const;
    bool tryMapScreenToSphere(const QPointF &position, bool preferRearHemisphere, QVector3D &spherePoint) const;
    bool tryMapScreenToSphere(const QPointF &position,
                              bool preferRearHemisphere,
                              bool requireVisibleCap,
                              QVector3D &spherePoint) const;
    void updateSurfaceTracking(const QPointF &position, bool preferRearHemisphere = false);
    void updateGlobalAxisTracking(const QPointF &position);
    void updateDrag(const QPointF &position);
    void emitStatus();
    void showContextMenu(const QPoint &globalPosition);
    bool isComponentValueLocked(FrameMath::ComponentId id) const;
    bool isGlobeOrientationLocked() const;

    FrameMath::State m_state;
    FrameMath::ComponentLengths m_components;
    float m_globeRadius = FrameMath::kSphereRadius;
    float m_zoom = 1.0f;
    float m_capPolarAngle = 180.0f;
    QQuaternion m_globeOrientation{1.0f, 0.0f, 0.0f, 0.0f};
    QVector3D m_cameraEyeOffset{4.0f, 4.0f, 4.0f};
    QVector3D m_cameraUpVector{0.0f, 0.0f, 1.0f};
    float m_perspectiveFovY = 35.0f;
    QImage m_projectionImage;
    QString m_projectionImagePath;
    QPointF m_projectionCircleCenter;
    float m_projectionCircleRadius = 0.0f;
    bool m_hasActiveComponent = true;
    bool m_projectionTextureDirty = true;
    DragState m_drag;
    LockState m_locks;
    QMatrix4x4 m_viewProjection;
    QScopedPointer<QOpenGLShaderProgram> m_program;
    QScopedPointer<QOpenGLShaderProgram> m_texturedCylinderProgram;
    QScopedPointer<QOpenGLTexture> m_projectionTexture;
    QOpenGLBuffer m_vertexBuffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_texturedCylinderVertexBuffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    QOpenGLVertexArrayObject m_texturedCylinderVao;
};

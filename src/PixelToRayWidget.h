#pragma once

#include "FrameMath.h"
#include "InteractionLogic.h"

#include <QMenu>
#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPointF>
#include <QScopedPointer>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <vector>

class PixelToRayWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PixelToRayWidget(QWidget *parent = nullptr);
    ~PixelToRayWidget() override;

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

    struct ProjectedPoint {
        QPointF screen;
        float depth = 0.0f;
    };

    struct DragState {
        bool active = false;
        FrameMath::ComponentId component = FrameMath::ComponentId::PositiveX;
        float startLength = 0.0f;
        QPointF startMouse;
    };

    struct HandleVisual {
        QPointF center;
        float radius = 0.0f;
        bool visible = false;
    };

    FrameMath::Frame currentFrame() const;
    QVector3D currentOrigin() const;
    QVector3D currentSceneCenter() const;
    QVector3D cameraPosition() const;
    float pixelsPerUnit() const;
    void updateViewProjection();
    void ensureGlResources();
    void appendLine(std::vector<Vertex> &vertices, const QVector3D &a, const QVector3D &b, const QColor &color, float alpha = 1.0f) const;
    void appendTriangle(std::vector<Vertex> &vertices, const QVector3D &a, const QVector3D &b, const QVector3D &c, const QColor &color, float alpha = 1.0f) const;
    void appendSphereSurface(std::vector<Vertex> &vertices) const;
    void appendSphereWireframe(std::vector<Vertex> &vertices) const;
    void appendFrameGeometry(std::vector<Vertex> &lineVertices, std::vector<Vertex> &pointVertices) const;
    void drawVertices(GLenum mode, const std::vector<Vertex> &vertices, float lineWidth = 1.0f, float pointSize = 1.0f);
    ProjectedPoint projectPoint(const QVector3D &worldPoint) const;
    QVector2D projectVectorToScreen(const QVector3D &origin, const QVector3D &direction, float length = 1.0f) const;
    HandleVisual handleVisual(FrameMath::ComponentId id) const;
    std::vector<InteractionLogic::HandleCandidate> handleCandidates() const;
    bool tryPickHandle(const QPointF &position, FrameMath::ComponentId &picked) const;
    void updateDrag(const QPointF &position);
    void emitStatus();
    void showContextMenu(const QPoint &globalPosition);

    FrameMath::State m_state;
    FrameMath::ComponentLengths m_components{};
    DragState m_drag;
    float m_zoom = 1.0f;
    QMatrix4x4 m_viewProjection;
    QScopedPointer<QOpenGLShaderProgram> m_program;
    QOpenGLBuffer m_vertexBuffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
};

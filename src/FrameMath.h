#pragma once

#include <QColor>
#include <QVector2D>
#include <QVector3D>

#include <array>

namespace FrameMath {

enum class ComponentId {
    PositiveX = 0,
    NegtiveX,
    PositiveY,
    NegtiveY,
    PositiveZ,
    NegtiveZ,
    Count
};

struct Frame {
    QVector3D xAxis;
    QVector3D yAxis;
    QVector3D zAxis;
};

struct ComponentMeta {
    char axis;
    int sign;
    const char *key;
};

struct State {
    float latitude = 20.0f;
    float longitude = 35.0f;
    float roll = 0.0f;
    ComponentId active = ComponentId::PositiveX;
    bool showRearWireframe = true;
    bool showLocalFrame = true;
    bool fixGlobalCenter = true;
};

constexpr float kSphereRadius = 2.0f;
constexpr float kMinLength = 0.08f;
constexpr float kMaxLength = 1.5f;
constexpr float kFrameVisualLength = 0.55f;

using ComponentLengths = std::array<float, static_cast<int>(ComponentId::Count)>;

float clampLength(float value, float minValue = kMinLength, float maxValue = kMaxLength);
float projectDeltaOntoDirection(const QVector3D &startPoint, const QVector3D &currentPoint, const QVector3D &direction);
QVector3D latLonToVector(float latitudeDeg, float longitudeDeg, float radius);
Frame getLocalFrame(float latitudeDeg, float longitudeDeg, float rollDeg = 0.0f);
ComponentMeta componentMeta(ComponentId id);
QVector3D componentDirection(const Frame &frame, ComponentId id);
QColor axisColor(char axis);
const char *componentKey(ComponentId id);
void runSanityChecks();

}  // namespace FrameMath

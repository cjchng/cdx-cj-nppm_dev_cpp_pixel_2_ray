#include "FrameMath.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace FrameMath {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float degToRad(float degrees) {
    return degrees * kPi / 180.0f;
}

QVector3D normalizeSafe(const QVector3D &value) {
    if (qFuzzyIsNull(value.lengthSquared())) {
        return {};
    }
    return value.normalized();
}

QVector3D rotateBasis(const QVector3D &east, const QVector3D &north, float rollDeg) {
    const float roll = degToRad(rollDeg);
    const float cosRoll = std::cos(roll);
    const float sinRoll = std::sin(roll);
    return normalizeSafe(east * cosRoll + north * sinRoll);
}

}  // namespace

float clampLength(float value, float minValue, float maxValue) {
    return std::clamp(value, minValue, maxValue);
}

float projectDeltaOntoDirection(const QVector3D &startPoint, const QVector3D &currentPoint, const QVector3D &direction) {
    return QVector3D::dotProduct(currentPoint - startPoint, direction);
}

QVector3D latLonToVector(float latitudeDeg, float longitudeDeg, float radius) {
    const float latitude = degToRad(latitudeDeg);
    const float longitude = degToRad(longitudeDeg);
    const float cosLatitude = std::cos(latitude);

    return {
        radius * cosLatitude * std::cos(longitude),
        radius * cosLatitude * std::sin(longitude),
        radius * std::sin(latitude)
    };
}

Frame getLocalFrame(float latitudeDeg, float longitudeDeg, float rollDeg) {
    const float latitude = degToRad(latitudeDeg);
    const float longitude = degToRad(longitudeDeg);
    const QVector3D zAxis = normalizeSafe(latLonToVector(latitudeDeg, longitudeDeg, 1.0f));
    const QVector3D east = normalizeSafe({-std::sin(longitude), std::cos(longitude), 0.0f});
    const QVector3D north = normalizeSafe({
        -std::sin(latitude) * std::cos(longitude),
        -std::sin(latitude) * std::sin(longitude),
        std::cos(latitude)
    });

    const QVector3D xAxis = rotateBasis(east, north, rollDeg);
    const QVector3D yAxis = normalizeSafe(QVector3D::crossProduct(zAxis, xAxis));
    return {xAxis, yAxis, zAxis};
}

ComponentMeta componentMeta(ComponentId id) {
    switch (id) {
    case ComponentId::PositiveX:
        return {'x', 1, "positive_x"};
    case ComponentId::NegtiveX:
        return {'x', -1, "negtive_x"};
    case ComponentId::PositiveY:
        return {'y', 1, "positive_y"};
    case ComponentId::NegtiveY:
        return {'y', -1, "negtive_y"};
    case ComponentId::PositiveZ:
        return {'z', 1, "positive_z"};
    case ComponentId::NegtiveZ:
        return {'z', -1, "negtive_z"};
    case ComponentId::Count:
        break;
    }

    return {'x', 1, "positive_x"};
}

QVector3D componentDirection(const Frame &frame, ComponentId id) {
    const ComponentMeta meta = componentMeta(id);
    const QVector3D base = meta.axis == 'x' ? frame.xAxis
                          : meta.axis == 'y' ? frame.yAxis
                                             : frame.zAxis;
    return base * static_cast<float>(meta.sign);
}

QColor axisColor(char axis) {
    switch (axis) {
    case 'x':
        return QColor(0xff, 0x55, 0x55);
    case 'y':
        return QColor(0x55, 0xff, 0x55);
    case 'z':
        return QColor(0x55, 0x99, 0xff);
    default:
        return Qt::white;
    }
}

const char *componentKey(ComponentId id) {
    return componentMeta(id).key;
}

void runSanityChecks() {
    const QVector3D equator = latLonToVector(0.0f, 0.0f, 2.0f);
    Q_ASSERT(std::abs(equator.x() - 2.0f) < 1e-5f);
    Q_ASSERT(std::abs(equator.y()) < 1e-5f);
    Q_ASSERT(std::abs(equator.z()) < 1e-5f);

    const QVector3D northPole = latLonToVector(90.0f, 0.0f, 2.0f);
    Q_ASSERT(std::abs(northPole.z() - 2.0f) < 1e-5f);

    const Frame frame = getLocalFrame(0.0f, 0.0f, 0.0f);
    Q_ASSERT(std::abs(frame.xAxis.x()) < 1e-5f && std::abs(frame.xAxis.y() - 1.0f) < 1e-5f);
    Q_ASSERT(std::abs(frame.yAxis.z() - 1.0f) < 1e-5f);
    Q_ASSERT(std::abs(frame.zAxis.x() - 1.0f) < 1e-5f);
}

}  // namespace FrameMath

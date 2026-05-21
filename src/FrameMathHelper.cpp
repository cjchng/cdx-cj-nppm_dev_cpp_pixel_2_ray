//
// Created by 張創然 on 2026/5/20.
//
#include "FrameMath.h"
#include "FrameMathHelper.h"
// cpp
//#include <QVector3D>
#include <cmath>
#include <algorithm>

//struct Spherical {
//    float latDeg; // latitude in degrees (-90..90)
//    float lonDeg; // longitude in degrees
//    float r;      // radius
//};

//static inline float deg2rad(float d) { return d * static_cast<float>(M_PI) / 180.0f; }
//static inline float rad2deg(float r) { return r * 180.0f / static_cast<float>(M_PI); }
namespace {
    constexpr float PI_F = 3.14159265358979323846f;
    static inline float deg2rad(float d) { return d * (PI_F / 180.0f); }
    static inline float rad2deg(float r) { return r * (180.0f / PI_F); }
}

// Convert spherical (latitude, longitude, radius) -> Cartesian (X, Y, Z).
// Convention: latitude = 0 at equator, +90 at north pole; Y is up.
QVector3D sphericalToCartesian(float latDeg, float lonDeg, float radius ) {
    const float phi = deg2rad(latDeg);   // latitude
    const float lambda = deg2rad(lonDeg); // longitude

    const float cosPhi = std::cos(phi);
    const float x = radius * cosPhi * std::cos(lambda);
    const float y = radius * std::sin(phi);
    const float z = radius * cosPhi * std::sin(lambda);
    return QVector3D(x, y, z);
}

// Convert Cartesian -> spherical (latitude, longitude, radius).
Spherical cartesianToSpherical(const QVector3D &p) {
    const float r = p.length();
    if (r == 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    const float lat = std::asin(p.y() / r);            // latitude in radians
    const float lon = std::atan2(p.z(), p.x());        // longitude in radians
    return { rad2deg(lat), rad2deg(lon), r };
}

// Build a right-handed local frame at the sphere point given lat/lon.
// Returns normalized normal (outward), east (increasing longitude), north (increasing latitude).
void localFrameFromSpherical(float latDeg, float lonDeg,
                             QVector3D &outNormal,
                             QVector3D &outEast,
                             QVector3D &outNorth) {
    // Position on unit sphere
    QVector3D p = sphericalToCartesian(latDeg, lonDeg, 1.0f);
    outNormal = p.normalized();

    // East direction (∂p / ∂λ normalized): points toward increasing longitude
    // dp/dλ = (-cosφ * sinλ, 0, cosφ * cosλ) * r  -> normalized reduces to (-sinλ, 0, cosλ)
    const float lambda = deg2rad(lonDeg);
    QVector3D east(-std::sin(lambda), 0.0f, std::cos(lambda));
    if (east.lengthSquared() < 1e-8f) {
        // at poles, choose an arbitrary east (along +X)
        east = QVector3D(1.0f, 0.0f, 0.0f);
    }
    outEast = east.normalized();

    // North direction (∂p / ∂φ normalized): points toward increasing latitude
    // dp/dφ = (-sinφ cosλ, cosφ, -sinφ sinλ) * r
    const float phi = deg2rad(latDeg);
    QVector3D north(-std::sin(phi) * std::cos(lambda),
                     std::cos(phi),
                    -std::sin(phi) * std::sin(lambda));
    outNorth = north.normalized();

    // Ensure orthonormality (re-orthogonalize if necessary)
    outEast = (outEast - outNormal * QVector3D::dotProduct(outNormal, outEast)).normalized();
    outNorth = QVector3D::crossProduct(outNormal, outEast); // east × normal -> north (right-handed)
    // Now outNormal, outEast, outNorth form a right-handed orthonormal basis.
}

// Add this helper to PixelToRayWidget (implementation file)

//
// Created by 張創然 on 2026/5/20.
//

// FrameMathHelper.h
#ifndef FRAMEMATHHELPER_H
#define FRAMEMATHHELPER_H

#include <QVector3D>

struct Spherical {
    float latDeg; // latitude in degrees (-90..90)
    float lonDeg; // longitude in degrees
    float r;      // radius
};

// Convert spherical (latitude, longitude, radius) -> Cartesian (X, Y, Z).
// Convention: latitude = 0 at equator, +90 at north pole; Y is up.
QVector3D sphericalToCartesian(float latDeg, float lonDeg, float radius = 1.0f);

// Convert Cartesian -> spherical (latitude, longitude, radius).
Spherical cartesianToSpherical(const QVector3D &p);

// Build a right-handed local frame at the sphere point given lat/lon.
// Returns normalized normal (outward), east (increasing longitude), north (increasing latitude).
void localFrameFromSpherical(float latDeg, float lonDeg,
                             QVector3D &outNormal,
                             QVector3D &outEast,
                             QVector3D &outNorth);

#endif // FRAMEMATHHELPER_H

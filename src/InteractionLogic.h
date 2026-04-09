#pragma once

#include "FrameMath.h"

#include <QPointF>
#include <QVector2D>

#include <vector>

namespace InteractionLogic {

struct HandleCandidate {
    FrameMath::ComponentId id = FrameMath::ComponentId::PositiveX;
    QPointF center;
    float radius = 0.0f;
    bool visible = false;
};

bool tryPickHandle(const QPointF &position, const std::vector<HandleCandidate> &candidates, FrameMath::ComponentId &picked);
float lengthDeltaFromScreenDrag(const QPointF &startMouse, const QPointF &currentMouse, const QVector2D &axisScreen);

}  // namespace InteractionLogic

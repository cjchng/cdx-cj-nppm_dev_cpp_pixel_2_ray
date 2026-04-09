#include "InteractionLogic.h"

#include <cmath>

namespace InteractionLogic {

bool tryPickHandle(const QPointF &position, const std::vector<HandleCandidate> &candidates, FrameMath::ComponentId &picked) {
    for (const HandleCandidate &candidate : candidates) {
        if (!candidate.visible) {
            continue;
        }

        if (QVector2D(position - candidate.center).length() <= candidate.radius + 2.0f) {
            picked = candidate.id;
            return true;
        }
    }

    return false;
}

float lengthDeltaFromScreenDrag(const QPointF &startMouse, const QPointF &currentMouse, const QVector2D &axisScreen) {
    const float axisLength = axisScreen.length();
    if (axisLength < 1e-4f) {
        return 0.0f;
    }

    const QVector2D screenDelta(currentMouse - startMouse);
    const float signedPixels = QVector2D::dotProduct(screenDelta, axisScreen.normalized());
    return signedPixels / axisLength;
}

}  // namespace InteractionLogic

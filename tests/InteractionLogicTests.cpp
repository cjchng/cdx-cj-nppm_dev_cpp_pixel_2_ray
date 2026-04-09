#include "InteractionLogic.h"

#include <QtTest/QtTest>

#include <vector>

class InteractionLogicTests : public QObject {
    Q_OBJECT

private slots:
    void tryPickHandle_ignoresHiddenHandles();
    void tryPickHandle_returnsNearestVisibleHit();
    void lengthDeltaFromScreenDrag_projectsAlongAxis();
    void lengthDeltaFromScreenDrag_returnsZeroForDegenerateAxis();
};

void InteractionLogicTests::tryPickHandle_ignoresHiddenHandles() {
    std::vector<InteractionLogic::HandleCandidate> candidates = {
        {FrameMath::ComponentId::PositiveX, QPointF(20.0, 20.0), 8.0f, false},
        {FrameMath::ComponentId::PositiveY, QPointF(80.0, 80.0), 8.0f, true}
    };

    FrameMath::ComponentId picked = FrameMath::ComponentId::PositiveX;
    QVERIFY(!InteractionLogic::tryPickHandle(QPointF(20.0, 20.0), candidates, picked));
    QCOMPARE(picked, FrameMath::ComponentId::PositiveX);
}

void InteractionLogicTests::tryPickHandle_returnsNearestVisibleHit() {
    std::vector<InteractionLogic::HandleCandidate> candidates = {
        {FrameMath::ComponentId::PositiveX, QPointF(20.0, 20.0), 8.0f, true},
        {FrameMath::ComponentId::NegtiveZ, QPointF(40.0, 40.0), 8.0f, true}
    };

    FrameMath::ComponentId picked = FrameMath::ComponentId::PositiveX;
    QVERIFY(InteractionLogic::tryPickHandle(QPointF(42.0, 41.0), candidates, picked));
    QCOMPARE(picked, FrameMath::ComponentId::NegtiveZ);
}

void InteractionLogicTests::lengthDeltaFromScreenDrag_projectsAlongAxis() {
    const float delta = InteractionLogic::lengthDeltaFromScreenDrag(
        QPointF(10.0, 10.0),
        QPointF(34.0, 18.0),
        QVector2D(20.0f, 0.0f)
    );

    QVERIFY(std::abs(delta - 1.2f) < 1e-5f);
}

void InteractionLogicTests::lengthDeltaFromScreenDrag_returnsZeroForDegenerateAxis() {
    const float delta = InteractionLogic::lengthDeltaFromScreenDrag(
        QPointF(10.0, 10.0),
        QPointF(34.0, 18.0),
        QVector2D(0.0f, 0.0f)
    );

    QCOMPARE(delta, 0.0f);
}

QTEST_APPLESS_MAIN(InteractionLogicTests)

#include "InteractionLogicTests.moc"

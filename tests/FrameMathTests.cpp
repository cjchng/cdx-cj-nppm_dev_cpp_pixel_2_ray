#include "FrameMath.h"

#include <QtTest/QtTest>

namespace {

bool approx(float actual, float expected, float tolerance = 1e-5f) {
    return std::abs(actual - expected) <= tolerance;
}

bool approxVector(const QVector3D &actual, const QVector3D &expected, float tolerance = 1e-5f) {
    return approx(actual.x(), expected.x(), tolerance)
        && approx(actual.y(), expected.y(), tolerance)
        && approx(actual.z(), expected.z(), tolerance);
}

}  // namespace

class FrameMathTests : public QObject {
    Q_OBJECT

private slots:
    void latLonToVector_mapsEquatorAndPole();
    void getLocalFrame_returnsExpectedBasis();
    void getLocalFrame_appliesRoll();
    void componentDirection_preservesAxisAndSign();
    void projectDeltaOntoDirection_returnsSignedMovement();
    void clampLength_enforcesLimits();
};

void FrameMathTests::latLonToVector_mapsEquatorAndPole() {
    QVERIFY(approxVector(FrameMath::latLonToVector(0.0f, 0.0f, 2.0f), QVector3D(2.0f, 0.0f, 0.0f)));
    QVERIFY(approxVector(FrameMath::latLonToVector(90.0f, 0.0f, 2.0f), QVector3D(0.0f, 0.0f, 2.0f)));
}

void FrameMathTests::getLocalFrame_returnsExpectedBasis() {
    const FrameMath::Frame frame = FrameMath::getLocalFrame(0.0f, 0.0f, 0.0f);
    QVERIFY(approxVector(frame.xAxis, QVector3D(0.0f, 1.0f, 0.0f)));
    QVERIFY(approxVector(frame.yAxis, QVector3D(0.0f, 0.0f, 1.0f)));
    QVERIFY(approxVector(frame.zAxis, QVector3D(1.0f, 0.0f, 0.0f)));
}

void FrameMathTests::getLocalFrame_appliesRoll() {
    const FrameMath::Frame frame = FrameMath::getLocalFrame(0.0f, 0.0f, 90.0f);
    QVERIFY(approxVector(frame.xAxis, QVector3D(0.0f, 0.0f, 1.0f)));
    QVERIFY(approxVector(frame.yAxis, QVector3D(0.0f, -1.0f, 0.0f)));
    QVERIFY(approxVector(frame.zAxis, QVector3D(1.0f, 0.0f, 0.0f)));
}

void FrameMathTests::componentDirection_preservesAxisAndSign() {
    const FrameMath::Frame frame = FrameMath::getLocalFrame(0.0f, 0.0f, 0.0f);
    const FrameMath::ComponentMeta meta = FrameMath::componentMeta(FrameMath::ComponentId::NegtiveZ);

    QCOMPARE(meta.axis, 'z');
    QCOMPARE(meta.sign, -1);
    QVERIFY(approxVector(
        FrameMath::componentDirection(frame, FrameMath::ComponentId::PositiveX),
        QVector3D(0.0f, 1.0f, 0.0f)
    ));
    QVERIFY(approxVector(
        FrameMath::componentDirection(frame, FrameMath::ComponentId::NegtiveX),
        QVector3D(0.0f, -1.0f, 0.0f)
    ));
}

void FrameMathTests::projectDeltaOntoDirection_returnsSignedMovement() {
    const float delta = FrameMath::projectDeltaOntoDirection(
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(0.25f, 0.1f, 0.0f),
        QVector3D(1.0f, 0.0f, 0.0f)
    );

    QVERIFY(approx(delta, 0.25f));
}

void FrameMathTests::clampLength_enforcesLimits() {
    QCOMPARE(FrameMath::clampLength(-1.0f), FrameMath::kMinLength);
    QCOMPARE(FrameMath::clampLength(0.5f), 0.5f);
    QCOMPARE(FrameMath::clampLength(3.0f), FrameMath::kMaxLength);
}

QTEST_APPLESS_MAIN(FrameMathTests)

#include "FrameMathTests.moc"

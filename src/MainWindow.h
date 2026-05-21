#pragma once

#include "FrameMath.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMainWindow>

class PixelToRayWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void syncInfoPanel();
    QDoubleSpinBox *createSpinBox(double minValue, double maxValue, double step, int decimals);
    QWidget *createLockableEditor(QDoubleSpinBox *spin, QCheckBox **lockCheck);

    PixelToRayWidget *m_view = nullptr;
    QLabel *m_activeLabel = nullptr;
    QLabel *m_centerLabel = nullptr;
    QLabel *m_yTraceLabel = nullptr;
    QLabel *m_xCylinderLabel = nullptr;
    QDoubleSpinBox *m_latitudeSpin = nullptr;
    QDoubleSpinBox *m_longitudeSpin = nullptr;
    QDoubleSpinBox *m_rollSpin = nullptr;
    QDoubleSpinBox *m_zoomSpin = nullptr;
    QDoubleSpinBox *m_capPolarAngleSpin = nullptr;
    QDoubleSpinBox *m_positiveXSpin = nullptr;
    QDoubleSpinBox *m_negtiveXSpin = nullptr;
    QDoubleSpinBox *m_positiveYSpin = nullptr;
    QDoubleSpinBox *m_negtiveYSpin = nullptr;
    QDoubleSpinBox *m_positiveZSpin = nullptr;
    QDoubleSpinBox *m_negtiveZSpin = nullptr;
    QCheckBox *m_latitudeLock = nullptr;
    QCheckBox *m_longitudeLock = nullptr;
    QCheckBox *m_rollLock = nullptr;
    QCheckBox *m_zoomLock = nullptr;
    QCheckBox *m_capPolarAngleLock = nullptr;
    QCheckBox *m_positiveXLock = nullptr;
    QCheckBox *m_negtiveXLock = nullptr;
    QCheckBox *m_positiveYLock = nullptr;
    QCheckBox *m_negtiveYLock = nullptr;
    QCheckBox *m_positiveZLock = nullptr;
    bool m_syncingInfoPanel = false;
};

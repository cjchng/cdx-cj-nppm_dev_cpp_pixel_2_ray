#pragma once

#include "FrameMath.h"

#include <QQuaternion>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QString>

class PixelToRayWidget;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    struct PanelState {
        float latitude = 20.0f;
        float longitude = 35.0f;
        float roll = 0.0f;
        float zoom = 1.0f;
        float capPolarAngle = 180.0f;
        float globeRadius = FrameMath::kSphereRadius;
        QQuaternion globeOrientation{1.0f, 0.0f, 0.0f, 0.0f};
        QString fisheyeImagePath;
        QString profileComment;
        float perspectiveFovY = 35.0f;
        float cameraEyeOffsetX = 4.0f;
        float cameraEyeOffsetY = 4.0f;
        float cameraEyeOffsetZ = 4.0f;
        float cameraUpX = 0.0f;
        float cameraUpY = 0.0f;
        float cameraUpZ = 1.0f;
        bool rearHemisphereWireframeVisible = true;
        bool worldAxesVisible = true;
        bool localCoordinateFrameVisible = true;
        bool globalCenterFixed = true;
        bool panoramaBandVisible = true;
        bool xTangentCylinderVisible = true;
        bool surfaceDepthHidingEnabled = true;
        bool capClosed = true;
        bool perspectiveProjection = false;
        float positiveX = 0.5f;
        float negtiveX = 0.3f;
        float positiveY = 0.5f;
        float negtiveY = 0.3f;
        float positiveZ = 0.5f;
        bool latitudeLocked = false;
        bool longitudeLocked = false;
        bool rollLocked = false;
        bool zoomLocked = false;
        bool capPolarAngleLocked = false;
        bool globeRadiusLocked = true;
        bool globeOrientationScalarLocked = false;
        bool globeOrientationXLocked = false;
        bool globeOrientationYLocked = false;
        bool globeOrientationZLocked = false;
        bool cameraEyeOffsetXLocked = false;
        bool cameraEyeOffsetYLocked = false;
        bool cameraEyeOffsetZLocked = false;
        bool cameraUpXLocked = false;
        bool cameraUpYLocked = false;
        bool cameraUpZLocked = false;
        bool positiveXLocked = false;
        bool negtiveXLocked = false;
        bool positiveYLocked = false;
        bool negtiveYLocked = false;
        bool positiveZLocked = false;
    };

    void loadStartupSettings();
    PanelState capturePanelState() const;
    void applyPanelState(const PanelState &state);
    void applyDefaultPanelState();
    void applySavedPanelState();
    void saveStateFiles();
    void configureStateFileWatcher();
    void reloadExternalStateFiles();
    void syncInfoPanel();
    void installUiFontShortcuts();
    void adjustUiFontScale(double delta);
    void resetUiFontScale();
    void applyUiFontScale();
    void applyUiFontScale(QWidget *widget);
    bool setShowingCurrentState(bool showCurrent, bool persistRuntimeState);
    bool readActiveProfileRuntimeState(bool fallbackShowCurrent) const;
    double readUiFontScaleRuntimeState(double fallbackScale) const;
    void saveActiveProfileRuntimeState() const;
    PanelState loadPanelStateFromJson(const QString &path, const PanelState &fallback) const;
    void savePanelStateToJson(const QString &path, const PanelState &state) const;
    QString stateProfilesDirectory() const;
    QString defaultStateJsonPath() const;
    QString currentStateJsonPath() const;
    QString activeProfileRuntimeJsonPath() const;
    QDoubleSpinBox *createSpinBox(double minValue, double maxValue, double step, int decimals);
    QWidget *createLockableEditor(QDoubleSpinBox *spin, QCheckBox **lockCheck);

    PixelToRayWidget *m_view = nullptr;
    QLabel *m_activeLabel = nullptr;
    QPushButton *m_currentHeaderButton = nullptr;
    QPushButton *m_defaultHeaderButton = nullptr;
    QPushButton *m_centerButton = nullptr;
    QPushButton *m_yTraceButton = nullptr;
    QPushButton *m_xCylinderButton = nullptr;
    QPushButton *m_surfaceDepthButton = nullptr;
    QPushButton *m_capClosedButton = nullptr;
    QPushButton *m_projectionButton = nullptr;
    QLabel *m_fisheyeImageLabel = nullptr;
    QDoubleSpinBox *m_latitudeSpin = nullptr;
    QDoubleSpinBox *m_longitudeSpin = nullptr;
    QDoubleSpinBox *m_rollSpin = nullptr;
    QDoubleSpinBox *m_zoomSpin = nullptr;
    QDoubleSpinBox *m_capPolarAngleSpin = nullptr;
    QDoubleSpinBox *m_globeRadiusSpin = nullptr;
    QDoubleSpinBox *m_globeOrientationWSpin = nullptr;
    QDoubleSpinBox *m_globeOrientationXSpin = nullptr;
    QDoubleSpinBox *m_globeOrientationYSpin = nullptr;
    QDoubleSpinBox *m_globeOrientationZSpin = nullptr;
    QCheckBox *m_globeOrientationWLock = nullptr;
    QCheckBox *m_globeOrientationXLock = nullptr;
    QCheckBox *m_globeOrientationYLock = nullptr;
    QCheckBox *m_globeOrientationZLock = nullptr;
    QDoubleSpinBox *m_eulerAxisXSpin = nullptr;
    QDoubleSpinBox *m_eulerAxisYSpin = nullptr;
    QDoubleSpinBox *m_eulerAxisZSpin = nullptr;
    QDoubleSpinBox *m_eulerAngleSpin = nullptr;
    QDoubleSpinBox *m_perspectiveFovYSpin = nullptr;
    QDoubleSpinBox *m_cameraEyeOffsetXSpin = nullptr;
    QDoubleSpinBox *m_cameraEyeOffsetYSpin = nullptr;
    QDoubleSpinBox *m_cameraEyeOffsetZSpin = nullptr;
    QDoubleSpinBox *m_cameraTargetXSpin = nullptr;
    QDoubleSpinBox *m_cameraTargetYSpin = nullptr;
    QDoubleSpinBox *m_cameraTargetZSpin = nullptr;
    QDoubleSpinBox *m_cameraUpXSpin = nullptr;
    QDoubleSpinBox *m_cameraUpYSpin = nullptr;
    QDoubleSpinBox *m_cameraUpZSpin = nullptr;
    QDoubleSpinBox *m_anchorXSpin = nullptr;
    QDoubleSpinBox *m_anchorYSpin = nullptr;
    QDoubleSpinBox *m_anchorZSpin = nullptr;
    QDoubleSpinBox *m_positiveXSpin = nullptr;
    QDoubleSpinBox *m_negtiveXSpin = nullptr;
    QDoubleSpinBox *m_positiveYSpin = nullptr;
    QDoubleSpinBox *m_vaSpin = nullptr;
    QDoubleSpinBox *m_negtiveYSpin = nullptr;
    QDoubleSpinBox *m_positiveZSpin = nullptr;
    QDoubleSpinBox *m_negtiveZSpin = nullptr;
    QCheckBox *m_latitudeLock = nullptr;
    QCheckBox *m_longitudeLock = nullptr;
    QCheckBox *m_rollLock = nullptr;
    QCheckBox *m_zoomLock = nullptr;
    QCheckBox *m_capPolarAngleLock = nullptr;
    QCheckBox *m_globeRadiusLock = nullptr;
    QCheckBox *m_cameraEyeOffsetXLock = nullptr;
    QCheckBox *m_cameraEyeOffsetYLock = nullptr;
    QCheckBox *m_cameraEyeOffsetZLock = nullptr;
    QCheckBox *m_cameraUpXLock = nullptr;
    QCheckBox *m_cameraUpYLock = nullptr;
    QCheckBox *m_cameraUpZLock = nullptr;
    QCheckBox *m_positiveXLock = nullptr;
    QCheckBox *m_negtiveXLock = nullptr;
    QCheckBox *m_positiveYLock = nullptr;
    QCheckBox *m_negtiveYLock = nullptr;
    QCheckBox *m_positiveZLock = nullptr;
    QFileSystemWatcher *m_stateFileWatcher = nullptr;
    QWidget *m_controlOverlayPanel = nullptr;
    QTimer *m_controlOverlayAutoHideTimer = nullptr;
    bool m_syncingInfoPanel = false;
    bool m_loadingSettings = false;
    bool m_controlOverlayCursorTrackingActive = false;
    PanelState m_defaultStateProfile;
    PanelState m_currentStateSnapshot;
    bool m_showingCurrentState = true;
    double m_baseUiFontPointSize = 10.0;
    double m_uiFontScale = 1.0;
};

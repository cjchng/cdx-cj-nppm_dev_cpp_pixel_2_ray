#include "MainWindow.h"

#include "PixelToRayWidget.h"

#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QMenuBar>
#include <QPalette>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWindow>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr auto kAssetsDirName = "assets";
constexpr auto kHfcImagesDirName = "hfc-images";
constexpr auto kLegacyStateProfilesDirName = "state_profiles";
constexpr auto kPresetStateFileName = "preset_state.json";
constexpr auto kLegacyDefaultStateFileName = "default_state.json";
constexpr auto kCurrentStateFileName = "current_state.json";
constexpr auto kRuntimeStateFileName = "ui_runtime_state.json";
constexpr auto kActiveProfileKey = "active_profile";
constexpr auto kUiFontScaleKey = "ui_font_scale";
constexpr auto kCurrentProfileName = "current";
constexpr auto kDefaultProfileName = "preset";
constexpr auto kLegacyDefaultProfileName = "default";
constexpr auto kProfileCommentKey = "profile_comment";
constexpr int kMaxPanelDecimals = 2;
constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;
constexpr double kMinUiFontScale = 0.65;
constexpr double kMaxUiFontScale = 1.8;
constexpr double kUiFontScaleStep = 0.1;
constexpr auto kBaseFontPointSizeProperty = "pixelToRayBaseFontPointSize";
constexpr int kControlOverlayBaseMinWidth = 760;
constexpr int kControlOverlayBaseMaxWidth = 860;
constexpr int kControlOverlayAutoHideIntervalMs = 150;

struct AxisAngleComponents {
    double axisX = 0.0;
    double axisY = 0.0;
    double axisZ = 1.0;
    double angleDegrees = 0.0;
};

struct ControlTabGroup {
    QString title;
    QVector<QWidget *> widgets;
};

bool widgetContainsGlobalPoint(QWidget *widget, const QPoint &globalPoint) {
    return widget != nullptr
        && widget->isVisible()
        && widget->rect().contains(widget->mapFromGlobal(globalPoint));
}

bool widgetContainsFocusedChild(QWidget *root, QWidget *focusedWidget) {
    for (QWidget *widget = focusedWidget; widget != nullptr; widget = widget->parentWidget()) {
        if (widget == root) {
            return true;
        }
    }
    return false;
}

QWidget *firstFocusableControlWidget(const QVector<QWidget *> &widgets, QWidget *fallback) {
    for (QWidget *widget : widgets) {
        if (widget == nullptr) {
            continue;
        }
        QList<QWidget *> candidates{widget};
        candidates.append(widget->findChildren<QWidget *>());
        for (QWidget *candidate : candidates) {
            if (candidate != nullptr
                && candidate->isEnabled()
                && candidate->isVisible()
                && candidate->focusPolicy() != Qt::NoFocus) {
                return candidate;
            }
        }
    }
    return fallback;
}

int panelDecimals(int decimals) {
    return std::clamp(decimals, 0, kMaxPanelDecimals);
}

double panelNumber(double value) {
    const double scale = std::pow(10.0, kMaxPanelDecimals);
    return std::round(value * scale) / scale;
}

AxisAngleComponents axisAngleFromQuaternion(const QQuaternion &orientation) {
    QQuaternion q = orientation.normalized();
    if (q.scalar() < 0.0f) {
        q = QQuaternion(-q.scalar(), -q.x(), -q.y(), -q.z());
    }

    const double x = q.x();
    const double y = q.y();
    const double z = q.z();
    const double vectorLength = std::sqrt(x * x + y * y + z * z);
    if (vectorLength < 1e-8) {
        return {};
    }

    return {
        panelNumber(x / vectorLength),
        panelNumber(y / vectorLength),
        panelNumber(z / vectorLength),
        panelNumber(2.0 * std::atan2(vectorLength, static_cast<double>(q.scalar())) * kRadiansToDegrees)
    };
}

double vaAngleDegrees(double positiveY, double globeRadius) {
    if (globeRadius <= 0.0) {
        return 0.0;
    }
    return panelNumber(std::atan2(std::max(0.0, positiveY), globeRadius) * kRadiansToDegrees);
}

QString resolveAssetsDirectoryPath() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::current().absoluteFilePath(QLatin1String(kAssetsDirName)),
        QDir(appDir).absoluteFilePath(QStringLiteral("../") + QLatin1String(kAssetsDirName)),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../") + QLatin1String(kAssetsDirName)),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../") + QLatin1String(kAssetsDirName)),
        QDir(appDir).absoluteFilePath(QLatin1String(kAssetsDirName))
    };

    for (const QString &path : candidates) {
        const QFileInfo info(path);
        if (info.isDir()) {
            return info.absoluteFilePath();
        }
    }

    return QDir(appDir).absoluteFilePath(QStringLiteral("../") + QLatin1String(kAssetsDirName));
}

QString resolveLegacyStateProfilesDirectoryPath() {
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(baseDir).filePath(QLatin1String(kLegacyStateProfilesDirName));
}

QString resolveHfcImagesDirectoryPath() {
    return QDir(resolveAssetsDirectoryPath()).absoluteFilePath(QLatin1String(kHfcImagesDirName));
}

QString legacyStateFilePath(QLatin1String fileName) {
    return QDir(resolveLegacyStateProfilesDirectoryPath()).filePath(fileName);
}

QString normalizedPathText(QString path) {
    return path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
}

bool relativePathStaysInsideBase(const QString &relativePath) {
    const QString normalized = normalizedPathText(relativePath);
    return !normalized.isEmpty()
        && normalized != QLatin1String(".")
        && normalized != QLatin1String("..")
        && !normalized.startsWith(QStringLiteral("../"))
        && !QDir::isAbsolutePath(normalized);
}

QString hfcRelativeImagePath(const QString &path) {
    const QString normalized = normalizedPathText(path);
    if (normalized.isEmpty()) {
        return QString();
    }

    const QString lower = normalized.toLower();
    const QString assetsHfcPrefix = QStringLiteral("assets/")
        + QString::fromLatin1(kHfcImagesDirName) + QLatin1Char('/');
    const QString hfcPrefix = QString::fromLatin1(kHfcImagesDirName) + QLatin1Char('/');
    if (lower == QStringLiteral("assets/") + QLatin1String(kHfcImagesDirName)
        || lower == QLatin1String(kHfcImagesDirName)) {
        return QString();
    }
    if (lower.startsWith(assetsHfcPrefix)) {
        return normalized.mid(assetsHfcPrefix.size()).trimmed();
    }
    if (lower.startsWith(hfcPrefix)) {
        return normalized.mid(hfcPrefix.size()).trimmed();
    }

    const QFileInfo info(normalized);
    if (info.isAbsolute()) {
        QString relativePath = QDir(resolveHfcImagesDirectoryPath()).relativeFilePath(info.absoluteFilePath());
        relativePath = normalizedPathText(relativePath);
        if (relativePathStaysInsideBase(relativePath)) {
            return relativePath;
        }
    }

    return normalized;
}

QString resolveFisheyeImagePath(const QString &storedPath) {
    const QString normalized = normalizedPathText(storedPath);
    if (normalized.isEmpty()) {
        return QString();
    }

    const QFileInfo info(normalized);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }

    const QString lower = normalized.toLower();
    const QString assetsPrefix = QStringLiteral("assets/");
    const QString hfcPrefix = QString::fromLatin1(kHfcImagesDirName) + QLatin1Char('/');
    if (lower == QLatin1String(kHfcImagesDirName) || lower.startsWith(hfcPrefix)) {
        return QFileInfo(QDir(resolveAssetsDirectoryPath()).absoluteFilePath(normalized)).absoluteFilePath();
    }
    if (lower == QStringLiteral("testing-images")
        || lower.startsWith(QStringLiteral("testing-images/"))) {
        return QFileInfo(QDir(resolveAssetsDirectoryPath()).absoluteFilePath(normalized)).absoluteFilePath();
    }
    if (lower.startsWith(assetsPrefix)) {
        return QFileInfo(QDir(resolveAssetsDirectoryPath()).absoluteFilePath(normalized.mid(assetsPrefix.size()))).absoluteFilePath();
    }

    return QFileInfo(QDir(resolveHfcImagesDirectoryPath()).absoluteFilePath(normalized)).absoluteFilePath();
}

QString resolveBuiltInFisheyePath() {
    const QString fileName = QStringLiteral("cj-circular.png");
    const QString assetsDir = resolveAssetsDirectoryPath();
    const QStringList candidates = {
        QDir(assetsDir).absoluteFilePath(QStringLiteral("hfc-images/") + fileName),
        QDir(assetsDir).absoluteFilePath(QStringLiteral("testing-images/") + fileName),
        QDir(assetsDir).absoluteFilePath(fileName),
        QDir::current().absoluteFilePath(fileName),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../") + fileName),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../") + fileName)
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return hfcRelativeImagePath(QFileInfo(path).absoluteFilePath());
        }
    }
    return QString();
}

QScreen *preferredStartupScreen(QMainWindow *window) {
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *leftmostScreen = nullptr;
    for (QScreen *screen : screens) {
        if (screen == nullptr) {
            continue;
        }
        if (leftmostScreen == nullptr) {
            leftmostScreen = screen;
            continue;
        }
        const QRect candidate = screen->geometry();
        const QRect selected = leftmostScreen->geometry();
        if (candidate.left() < selected.left()
            || (candidate.left() == selected.left() && candidate.top() < selected.top())) {
            leftmostScreen = screen;
        }
    }

    if (leftmostScreen != nullptr) {
        return leftmostScreen;
    }
    if (window != nullptr && window->screen() != nullptr) {
        return window->screen();
    }
    return QGuiApplication::primaryScreen();
}

void placeWindowNearTopRight(QMainWindow *window) {
    QScreen *screen = preferredStartupScreen(window);
    if (screen == nullptr) {
        return;
    }

    const QRect available = screen->availableGeometry();
    const QSize windowSize = window->size();
    const int margin = 32;
    const int x = available.right() - windowSize.width() - margin;
    const int y = available.top() + margin;

    window->move(std::max(available.left() + margin, x), y);
}

void showWindowOnPreferredScreen(QMainWindow *window) {
    QScreen *screen = preferredStartupScreen(window);
    if (screen == nullptr) {
        window->show();
        return;
    }

    window->winId();
    if (QWindow *handle = window->windowHandle()) {
        handle->setScreen(screen);
    }

    const QRect available = screen->availableGeometry();
    const int margin = 24;
    window->setGeometry(available.adjusted(margin, margin, -margin, -margin));
    window->show();
}

QWidget *createSectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: #f3f4f6; margin-top: 6px;"));
    return label;
}

QPushButton *createStatusToggleButton(QWidget *parent) {
    auto *button = new QPushButton(parent);
    button->setCheckable(true);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f2937; color: #e5e7eb; border: 1px solid #374151; padding: 6px 8px; text-align: left; }"
        "QPushButton:checked { background: #0f766e; border-color: #14b8a6; color: #f0fdfa; }"
    ));
    return button;
}

QPushButton *createHeaderModeButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #64748b; border: none; padding: 2px 0; font-weight: 600; text-align: left; }"
        "QPushButton:checked { color: #f8fafc; }"
    ));
    return button;
}

void configureReadOnlySpin(QDoubleSpinBox *spin) {
    spin->setReadOnly(true);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setFocusPolicy(Qt::NoFocus);
    spin->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background: #111827; color: #cbd5e1; border: 1px solid #1f2937; padding: 4px 6px; }"
    ));
}

QLabel *createReadOnlyValueLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setStyleSheet(QStringLiteral(
        "QLabel { background: #111827; color: #dbe4ee; border: 1px solid #1f2937; padding: 6px 8px; }"
    ));
    return label;
}

QWidget *createRotationDelimiter(QWidget *parent) {
    auto *label = new QLabel(QObject::tr("Euler axis-angle equivalent  |  read-only"), parent);
    label->setStyleSheet(QStringLiteral(
        "QLabel { background: #312e81; color: #fef3c7; border-left: 5px solid #f59e0b; padding: 7px 10px; font-weight: 700; }"
    ));
    return label;
}

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    m_baseUiFontPointSize = QApplication::font().pointSizeF();
    if (m_baseUiFontPointSize <= 0.0) {
        m_baseUiFontPointSize = font().pointSizeF() > 0.0 ? font().pointSizeF() : 10.0;
    }

    PanelState builtInDefaults;
    builtInDefaults.fisheyeImagePath = resolveBuiltInFisheyePath();
    const PanelState legacyDefaults = loadPanelStateFromJson(
        legacyStateFilePath(QLatin1String(kLegacyDefaultStateFileName)),
        builtInDefaults);
    const PanelState assetLegacyDefaults = loadPanelStateFromJson(
        QDir(stateProfilesDirectory()).filePath(QLatin1String(kLegacyDefaultStateFileName)),
        legacyDefaults);
    m_defaultStateProfile = loadPanelStateFromJson(defaultStateJsonPath(), assetLegacyDefaults);
    const PanelState legacyCurrent = loadPanelStateFromJson(
        legacyStateFilePath(QLatin1String(kCurrentStateFileName)),
        m_defaultStateProfile);
    m_currentStateSnapshot = loadPanelStateFromJson(currentStateJsonPath(), legacyCurrent);
    savePanelStateToJson(defaultStateJsonPath(), m_defaultStateProfile);
    savePanelStateToJson(currentStateJsonPath(), m_currentStateSnapshot);

    auto *central = new QWidget(this);
    auto *layout = new QGridLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_view = new PixelToRayWidget(central);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *panel = new QWidget(central);
    m_controlOverlayPanel = panel;
    panel->setObjectName(QStringLiteral("controlOverlayPanel"));
    panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    panel->setFocusPolicy(Qt::StrongFocus);
    panel->setMinimumWidth(kControlOverlayBaseMinWidth);
    panel->setMaximumWidth(kControlOverlayBaseMaxWidth);

    QPalette palette = panel->palette();
    palette.setColor(QPalette::Window, QColor(0x11, 0x18, 0x27));
    palette.setColor(QPalette::WindowText, QColor(0xe5, 0xe7, 0xeb));
    panel->setAutoFillBackground(true);
    panel->setPalette(palette);
    panel->setStyleSheet(QStringLiteral(
        "QWidget#controlOverlayPanel { background: rgba(17, 24, 39, 238); border: 1px solid #334155; }"
    ));

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(10);

    auto *title = new QLabel(tr("Control Table"), panel);
    QFont titleFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4.0);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: #f9fafb;"));
    auto *panelHeader = new QWidget(panel);
    auto *panelHeaderLayout = new QHBoxLayout(panelHeader);
    panelHeaderLayout->setContentsMargins(0, 0, 0, 0);
    panelHeaderLayout->setSpacing(10);
    auto *hidePanelButton = new QPushButton(tr("Hide"), panelHeader);
    hidePanelButton->setToolTip(tr("Hide the Control Table overlay"));
    hidePanelButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f2937; color: #e5e7eb; border: 1px solid #475569; padding: 5px 10px; }"
        "QPushButton:hover { background: #334155; }"
    ));
    panelHeaderLayout->addWidget(title, 1);
    panelHeaderLayout->addWidget(hidePanelButton, 0);
    panelLayout->addWidget(panelHeader);

    auto *grid = new QGridLayout();
    grid->setColumnMinimumWidth(0, 150);
    grid->setColumnStretch(1, 4);
    grid->setColumnStretch(2, 3);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);

    const PanelState defaults = m_defaultStateProfile;
    auto createDefaultSpin = [panel](double minValue, double maxValue, int decimals, double value) {
        auto *spin = new QDoubleSpinBox(panel);
        spin->setRange(minValue, maxValue);
        spin->setDecimals(panelDecimals(decimals));
        spin->setValue(panelNumber(value));
        configureReadOnlySpin(spin);
        return spin;
    };
    auto createDefaultLockLabel = [panel](bool locked) {
        return createReadOnlyValueLabel(locked ? tr("lock=on") : tr("lock=off"), panel);
    };
    auto createDefaultValueWithLock = [panel, &createDefaultLockLabel](QWidget *valueWidget, bool locked) {
        auto *container = new QWidget(panel);
        auto *rowLayout = new QHBoxLayout(container);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        rowLayout->addWidget(valueWidget, 1);
        rowLayout->addWidget(createDefaultLockLabel(locked), 0);
        return container;
    };
    QVector<ControlTabGroup> controlTabs;
    int activeControlTab = -1;
    auto rememberControlTabWidget = [&controlTabs, &activeControlTab](QWidget *widget) {
        if (widget != nullptr && activeControlTab >= 0 && activeControlTab < controlTabs.size()) {
            controlTabs[activeControlTab].widgets.append(widget);
        }
    };
    auto addSectionRow = [grid, panel, &controlTabs, &activeControlTab, &rememberControlTabWidget](int &row, const QString &title) {
        controlTabs.append({title, {}});
        activeControlTab = controlTabs.size() - 1;
        auto *label = qobject_cast<QLabel *>(createSectionLabel(title, panel));
        grid->addWidget(label, row++, 0, 1, 3);
        rememberControlTabWidget(label);
    };
    auto addFullWidthRow = [grid, &rememberControlTabWidget](int &row, QWidget *widget) {
        grid->addWidget(widget, row++, 0, 1, 3);
        rememberControlTabWidget(widget);
    };
    auto addTableHeaders = [this, grid, panel](int &row) {
        auto *fieldHeader = new QLabel(tr("Field"), panel);
        m_currentHeaderButton = createHeaderModeButton(tr("Current"), panel);
        m_defaultHeaderButton = createHeaderModeButton(tr("Preset"), panel);
        const QString style = QStringLiteral("color: #cbd5e1; font-weight: 600; padding: 2px 0;");
        fieldHeader->setStyleSheet(style);
        grid->addWidget(fieldHeader, row, 0);
        grid->addWidget(m_currentHeaderButton, row, 1);
        grid->addWidget(m_defaultHeaderButton, row, 2);
        ++row;
    };
    auto addRow = [grid, panel, &rememberControlTabWidget](int &row, const QString &name, QWidget *current, QWidget *deflt) {
        auto *field = new QLabel(name, panel);
        field->setStyleSheet(QStringLiteral("color: #e5e7eb;"));
        field->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(field, row, 0);
        grid->addWidget(current, row, 1);
        grid->addWidget(deflt, row, 2);
        rememberControlTabWidget(field);
        rememberControlTabWidget(current);
        rememberControlTabWidget(deflt);
        ++row;
    };

    int row = 0;
    addTableHeaders(row);

    m_activeLabel = new QLabel(panel);
    m_fisheyeImageLabel = new QLabel(panel);
    m_centerButton = createStatusToggleButton(panel);
    m_yTraceButton = createStatusToggleButton(panel);
    m_xCylinderButton = createStatusToggleButton(panel);
    m_surfaceDepthButton = createStatusToggleButton(panel);
    m_capClosedButton = createStatusToggleButton(panel);
    m_projectionButton = createStatusToggleButton(panel);
    m_activeLabel->setStyleSheet(QStringLiteral("color: #e5e7eb;"));
    m_fisheyeImageLabel->setStyleSheet(QStringLiteral("color: #cbd5e1;"));
    m_fisheyeImageLabel->setWordWrap(true);

    addSectionRow(row, tr("Status"));
    addRow(row, tr("Active"), m_activeLabel, createReadOnlyValueLabel(tr("positive_x  [x/X y/Y z/Z]"), panel));
    addRow(row, tr("Global Center"), m_centerButton, createReadOnlyValueLabel(defaults.globalCenterFixed ? tr("fixed") : tr("move"), panel));
    addRow(row, tr("y-axis great-circle trace"), m_yTraceButton, createReadOnlyValueLabel(defaults.panoramaBandVisible ? tr("on") : tr("off"), panel));
    addRow(row, tr("x-axis tangential cylinder"), m_xCylinderButton, createReadOnlyValueLabel(defaults.xTangentCylinderVisible ? tr("on") : tr("off"), panel));
    addRow(row, tr("Surface Depth Hiding"), m_surfaceDepthButton, createReadOnlyValueLabel(defaults.surfaceDepthHidingEnabled ? tr("on") : tr("off"), panel));
    addRow(row, tr("Cap Closed"), m_capClosedButton, createReadOnlyValueLabel(defaults.capClosed ? tr("on") : tr("off"), panel));
    addRow(row, tr("Projection"), m_projectionButton, createReadOnlyValueLabel(defaults.perspectiveProjection ? tr("perspective") : tr("orthographic"), panel));

    m_latitudeSpin = createSpinBox(-89.0, 89.0, 1.0, 2);
    m_longitudeSpin = createSpinBox(-9999.0, 9999.0, 1.0, 2);
    m_rollSpin = createSpinBox(-9999.0, 9999.0, 1.0, 2);
    m_zoomSpin = createSpinBox(0.35, 2.5, 0.05, 2);
    m_capPolarAngleSpin = createSpinBox(10.0, 300.0, 1.0, 1);

    addSectionRow(row, tr("View"));
    addRow(row, tr("Latitude"), createLockableEditor(m_latitudeSpin, &m_latitudeLock), createDefaultValueWithLock(createDefaultSpin(-89.0, 89.0, 2, defaults.latitude), defaults.latitudeLocked));
    addRow(row, tr("Longitude"), createLockableEditor(m_longitudeSpin, &m_longitudeLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 2, defaults.longitude), defaults.longitudeLocked));
    addRow(row, tr("Roll"), createLockableEditor(m_rollSpin, &m_rollLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 2, defaults.roll), defaults.rollLocked));
    addRow(row, tr("Zoom"), createLockableEditor(m_zoomSpin, &m_zoomLock), createDefaultValueWithLock(createDefaultSpin(0.35, 2.5, 2, defaults.zoom), defaults.zoomLocked));
    addRow(row, tr("Cap Angle"), createLockableEditor(m_capPolarAngleSpin, &m_capPolarAngleLock), createDefaultValueWithLock(createDefaultSpin(10.0, 300.0, 1, defaults.capPolarAngle), defaults.capPolarAngleLocked));

    m_globeRadiusSpin = createSpinBox(0.01, 9999.0, 0.01, 3);
    m_globeOrientationWSpin = createSpinBox(-1.0, 1.0, 0.01, 4);
    m_globeOrientationXSpin = createSpinBox(-1.0, 1.0, 0.01, 4);
    m_globeOrientationYSpin = createSpinBox(-1.0, 1.0, 0.01, 4);
    m_globeOrientationZSpin = createSpinBox(-1.0, 1.0, 0.01, 4);
    m_eulerAxisXSpin = createSpinBox(-1.0, 1.0, 0.0, 2);
    m_eulerAxisYSpin = createSpinBox(-1.0, 1.0, 0.0, 2);
    m_eulerAxisZSpin = createSpinBox(-1.0, 1.0, 0.0, 2);
    m_eulerAngleSpin = createSpinBox(0.0, 360.0, 0.0, 2);
    configureReadOnlySpin(m_eulerAxisXSpin);
    configureReadOnlySpin(m_eulerAxisYSpin);
    configureReadOnlySpin(m_eulerAxisZSpin);
    configureReadOnlySpin(m_eulerAngleSpin);
    m_perspectiveFovYSpin = createSpinBox(5.0, 140.0, 1.0, 1);
    m_anchorXSpin = createSpinBox(-9999.0, 9999.0, 0.0, 3);
    m_anchorYSpin = createSpinBox(-9999.0, 9999.0, 0.0, 3);
    m_anchorZSpin = createSpinBox(-9999.0, 9999.0, 0.0, 3);
    configureReadOnlySpin(m_anchorXSpin);
    configureReadOnlySpin(m_anchorYSpin);
    configureReadOnlySpin(m_anchorZSpin);

    addSectionRow(row, tr("Globe Coordinates"));
    addRow(row, tr("Globe Radius"), createLockableEditor(m_globeRadiusSpin, &m_globeRadiusLock), createDefaultValueWithLock(createDefaultSpin(0.01, 9999.0, 3, defaults.globeRadius), defaults.globeRadiusLocked));
    addRow(row, tr("Fisheye Image"), m_fisheyeImageLabel, createReadOnlyValueLabel(defaults.fisheyeImagePath.isEmpty() ? tr("none") : defaults.fisheyeImagePath, panel));
    addRow(row, tr("Anchor X"), m_anchorXSpin, createReadOnlyValueLabel(tr("derived from lat/lon"), panel));
    addRow(row, tr("Anchor Y"), m_anchorYSpin, createReadOnlyValueLabel(tr("derived from lat/lon"), panel));
    addRow(row, tr("Anchor Z"), m_anchorZSpin, createReadOnlyValueLabel(tr("derived from lat/lon"), panel));

    addSectionRow(row, tr("Global Orientation"));
    addRow(row,
           tr("Globe q_w"),
           createLockableEditor(m_globeOrientationWSpin, &m_globeOrientationWLock),
           createDefaultValueWithLock(createDefaultSpin(-1.0, 1.0, 4, defaults.globeOrientation.scalar()),
                                      defaults.globeOrientationScalarLocked));
    addRow(row,
           tr("Globe q_x"),
           createLockableEditor(m_globeOrientationXSpin, &m_globeOrientationXLock),
           createDefaultValueWithLock(createDefaultSpin(-1.0, 1.0, 4, defaults.globeOrientation.x()),
                                      defaults.globeOrientationXLocked));
    addRow(row,
           tr("Globe q_y"),
           createLockableEditor(m_globeOrientationYSpin, &m_globeOrientationYLock),
           createDefaultValueWithLock(createDefaultSpin(-1.0, 1.0, 4, defaults.globeOrientation.y()),
                                      defaults.globeOrientationYLocked));
    addRow(row,
           tr("Globe q_z"),
           createLockableEditor(m_globeOrientationZSpin, &m_globeOrientationZLock),
           createDefaultValueWithLock(createDefaultSpin(-1.0, 1.0, 4, defaults.globeOrientation.z()),
                                      defaults.globeOrientationZLocked));
    const AxisAngleComponents defaultAxisAngle = axisAngleFromQuaternion(defaults.globeOrientation);
    addFullWidthRow(row, createRotationDelimiter(panel));
    addRow(row, tr("Euler axis_x"), m_eulerAxisXSpin, createDefaultSpin(-1.0, 1.0, 2, defaultAxisAngle.axisX));
    addRow(row, tr("Euler axis_y"), m_eulerAxisYSpin, createDefaultSpin(-1.0, 1.0, 2, defaultAxisAngle.axisY));
    addRow(row, tr("Euler axis_z"), m_eulerAxisZSpin, createDefaultSpin(-1.0, 1.0, 2, defaultAxisAngle.axisZ));
    addRow(row, tr("Euler angle deg"), m_eulerAngleSpin, createDefaultSpin(0.0, 360.0, 2, defaultAxisAngle.angleDegrees));

    m_cameraEyeOffsetXSpin = createSpinBox(-9999.0, 9999.0, 0.1, 3);
    m_cameraEyeOffsetYSpin = createSpinBox(-9999.0, 9999.0, 0.1, 3);
    m_cameraEyeOffsetZSpin = createSpinBox(-9999.0, 9999.0, 0.1, 3);
    m_cameraTargetXSpin = createSpinBox(-9999.0, 9999.0, 0.0, 3);
    m_cameraTargetYSpin = createSpinBox(-9999.0, 9999.0, 0.0, 3);
    m_cameraTargetZSpin = createSpinBox(-9999.0, 9999.0, 0.0, 3);
    m_cameraUpXSpin = createSpinBox(-9999.0, 9999.0, 0.1, 3);
    m_cameraUpYSpin = createSpinBox(-9999.0, 9999.0, 0.1, 3);
    m_cameraUpZSpin = createSpinBox(-9999.0, 9999.0, 0.1, 3);
    configureReadOnlySpin(m_cameraTargetXSpin);
    configureReadOnlySpin(m_cameraTargetYSpin);
    configureReadOnlySpin(m_cameraTargetZSpin);

    addSectionRow(row, tr("Camera Pose"));
    addRow(row, tr("Perspective FOV Y"), m_perspectiveFovYSpin, createDefaultSpin(5.0, 140.0, 1, defaults.perspectiveFovY));
    addRow(row, tr("Eye Offset X"), createLockableEditor(m_cameraEyeOffsetXSpin, &m_cameraEyeOffsetXLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 3, defaults.cameraEyeOffsetX), defaults.cameraEyeOffsetXLocked));
    addRow(row, tr("Eye Offset Y"), createLockableEditor(m_cameraEyeOffsetYSpin, &m_cameraEyeOffsetYLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 3, defaults.cameraEyeOffsetY), defaults.cameraEyeOffsetYLocked));
    addRow(row, tr("Eye Offset Z"), createLockableEditor(m_cameraEyeOffsetZSpin, &m_cameraEyeOffsetZLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 3, defaults.cameraEyeOffsetZ), defaults.cameraEyeOffsetZLocked));
    addRow(row, tr("Target X"), m_cameraTargetXSpin, createReadOnlyValueLabel(tr("0.00 or anchor X"), panel));
    addRow(row, tr("Target Y"), m_cameraTargetYSpin, createReadOnlyValueLabel(tr("0.00 or anchor Y"), panel));
    addRow(row, tr("Target Z"), m_cameraTargetZSpin, createReadOnlyValueLabel(tr("0.00 or anchor Z"), panel));
    addRow(row, tr("Up X"), createLockableEditor(m_cameraUpXSpin, &m_cameraUpXLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 3, defaults.cameraUpX), defaults.cameraUpXLocked));
    addRow(row, tr("Up Y"), createLockableEditor(m_cameraUpYSpin, &m_cameraUpYLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 3, defaults.cameraUpY), defaults.cameraUpYLocked));
    addRow(row, tr("Up Z"), createLockableEditor(m_cameraUpZSpin, &m_cameraUpZLock), createDefaultValueWithLock(createDefaultSpin(-9999.0, 9999.0, 3, defaults.cameraUpZ), defaults.cameraUpZLocked));

    m_positiveXSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_negtiveXSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_positiveYSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_vaSpin = createSpinBox(0.0, 90.0, 0.1, 2);
    m_negtiveYSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_positiveZSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_negtiveZSpin = createSpinBox(0.01, 9999.0, 0.0, 3);
    m_vaSpin->setSuffix(tr(" deg"));
    configureReadOnlySpin(m_vaSpin);
    m_negtiveZSpin->setEnabled(false);

    addSectionRow(row, tr("Components"));
    addRow(row, tr("positive_x"), createLockableEditor(m_positiveXSpin, &m_positiveXLock), createDefaultValueWithLock(createDefaultSpin(FrameMath::kMinLength, FrameMath::kMaxLength, 3, defaults.positiveX), defaults.positiveXLocked));
    addRow(row, tr("negtive_x"), createLockableEditor(m_negtiveXSpin, &m_negtiveXLock), createDefaultValueWithLock(createDefaultSpin(FrameMath::kMinLength, FrameMath::kMaxLength, 3, defaults.negtiveX), defaults.negtiveXLocked));
    addRow(row, tr("positive_y"), createLockableEditor(m_positiveYSpin, &m_positiveYLock), createDefaultValueWithLock(createDefaultSpin(FrameMath::kMinLength, FrameMath::kMaxLength, 3, defaults.positiveY), defaults.positiveYLocked));
    auto *defaultVaSpin = createDefaultSpin(0.0, 90.0, 2, vaAngleDegrees(defaults.positiveY, defaults.globeRadius));
    defaultVaSpin->setSuffix(tr(" deg"));
    addRow(row, tr("va"), m_vaSpin, defaultVaSpin);
    addRow(row, tr("negtive_y"), createLockableEditor(m_negtiveYSpin, &m_negtiveYLock), createDefaultValueWithLock(createDefaultSpin(FrameMath::kMinLength, FrameMath::kMaxLength, 3, defaults.negtiveY), defaults.negtiveYLocked));
    addRow(row, tr("positive_z"), createLockableEditor(m_positiveZSpin, &m_positiveZLock), createDefaultValueWithLock(createDefaultSpin(FrameMath::kMinLength, FrameMath::kMaxLength, 3, defaults.positiveZ), defaults.positiveZLocked));
    addRow(row, tr("negtive_z"), m_negtiveZSpin, createReadOnlyValueLabel(tr("fixed to globe radius"), panel));

    panelLayout->addLayout(grid);

    auto *hintLabel = new QLabel(tr("Hints\n"
                                    "Control tabs: hover or Ctrl+1..Ctrl+6\n"
                                    "Overlay hides after pointer and focus leave\n"
                                    "ESC: quit\n"
                                    "Global Center / y-axis great-circle trace /\n"
                                    "x-axis tangential cylinder: right click in viewport"), panel);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: #cbd5e1;"));
    panelLayout->addWidget(createSectionLabel(tr("Shortcuts"), panel));
    panelLayout->addWidget(hintLabel);

    panelLayout->addStretch(1);

    layout->addWidget(m_view, 0, 0);
    layout->addWidget(panel, 0, 0, Qt::AlignLeft | Qt::AlignTop);
    panel->hide();
    setCentralWidget(central);

    m_controlOverlayAutoHideTimer = new QTimer(this);
    m_controlOverlayAutoHideTimer->setInterval(kControlOverlayAutoHideIntervalMs);
    connect(m_controlOverlayAutoHideTimer, &QTimer::timeout, this, [this]() {
        if (m_controlOverlayPanel == nullptr || !m_controlOverlayPanel->isVisible()) {
            m_controlOverlayAutoHideTimer->stop();
            return;
        }

        const QPoint cursorPosition = QCursor::pos();
        const bool cursorInPanel = widgetContainsGlobalPoint(m_controlOverlayPanel, cursorPosition);
        const bool cursorInMenuBar = widgetContainsGlobalPoint(menuBar(), cursorPosition);
        if (cursorInPanel) {
            m_controlOverlayCursorTrackingActive = true;
            return;
        }
        if (cursorInMenuBar) {
            return;
        }

        const bool focusInPanel = widgetContainsFocusedChild(m_controlOverlayPanel, QApplication::focusWidget());
        if (m_controlOverlayCursorTrackingActive || !focusInPanel) {
            if (focusInPanel && QApplication::focusWidget() != nullptr) {
                QApplication::focusWidget()->clearFocus();
            }
            m_controlOverlayPanel->hide();
            m_controlOverlayCursorTrackingActive = false;
            m_controlOverlayAutoHideTimer->stop();
        }
    });

    auto *commandsMenu = menuBar()->addMenu(tr("Commands"));
    QAction *setFisheyeImageAction = commandsMenu->addAction(tr("Set Fisheye Image..."));
    QAction *reloadFisheyeImageAction = commandsMenu->addAction(tr("Reload Fisheye Image"));
    QAction *clearFisheyeImageAction = commandsMenu->addAction(tr("Clear Fisheye Image"));

    auto applyControlTab = [this, controlTabs, panel, title](int index) {
        if (index < 0 || index >= controlTabs.size()) {
            return;
        }
        for (int groupIndex = 0; groupIndex < controlTabs.size(); ++groupIndex) {
            const bool visible = groupIndex == index;
            for (QWidget *widget : controlTabs[groupIndex].widgets) {
                if (widget != nullptr) {
                    widget->setVisible(visible);
                }
            }
        }
        title->setText(tr("Control Table - %1").arg(controlTabs[index].title));
        panel->show();
        panel->raise();
        panel->adjustSize();
        if (QWidget *focusTarget = firstFocusableControlWidget(controlTabs[index].widgets, panel)) {
            focusTarget->setFocus(Qt::TabFocusReason);
        }
        if (m_controlOverlayAutoHideTimer != nullptr && !m_controlOverlayAutoHideTimer->isActive()) {
            m_controlOverlayAutoHideTimer->start();
        }
    };
    connect(hidePanelButton, &QPushButton::clicked, this, [this]() {
        if (m_controlOverlayPanel != nullptr) {
            m_controlOverlayPanel->hide();
        }
        m_controlOverlayCursorTrackingActive = false;
        if (m_controlOverlayAutoHideTimer != nullptr) {
            m_controlOverlayAutoHideTimer->stop();
        }
    });

    menuBar()->setStyleSheet(QStringLiteral(
        "QMenuBar { background: #f3f4f6; color: #111827; }"
        "QMenuBar::item { color: #111827; padding: 2px 8px; }"
        "QMenuBar::item:selected { background: #dbeafe; color: #111827; }"
        "QMenuBar::item:checked { background: #111827; color: #f8fafc; }"
    ));
    auto *controlTabActionGroup = new QActionGroup(menuBar());
    controlTabActionGroup->setExclusive(true);
    for (int index = 0; index < controlTabs.size(); ++index) {
        const QString shortcutText = tr("Ctrl+%1").arg(index + 1);
        const QString tabText = tr("%1 [%2]").arg(controlTabs[index].title, shortcutText);
        QAction *tabAction = menuBar()->addAction(tabText);
        tabAction->setCheckable(true);
        tabAction->setShortcut(QKeySequence(shortcutText));
        tabAction->setShortcutContext(Qt::ApplicationShortcut);
        tabAction->setToolTip(tr("Show %1 controls (%2)").arg(controlTabs[index].title, shortcutText));
        controlTabActionGroup->addAction(tabAction);
        connect(menuBar(), &QMenuBar::hovered, this, [this, applyControlTab, tabAction, index](QAction *hoveredAction) {
            if (hoveredAction == tabAction) {
                m_controlOverlayCursorTrackingActive = true;
                tabAction->setChecked(true);
                applyControlTab(index);
            }
        });
        connect(tabAction, &QAction::triggered, this, [this, applyControlTab, index]() {
            m_controlOverlayCursorTrackingActive = widgetContainsGlobalPoint(menuBar(), QCursor::pos());
            applyControlTab(index);
        });
        if (index == 0) {
            tabAction->setChecked(true);
        }
    }
    installUiFontShortcuts();
    applyUiFontScale();

    connect(m_view, &PixelToRayWidget::statusTextChanged, this, [this](const QString &) {
        if (!m_loadingSettings) {
            if (m_showingCurrentState) {
                m_currentStateSnapshot = capturePanelState();
            } else {
                m_defaultStateProfile = capturePanelState();
            }
        }
        syncInfoPanel();
        saveStateFiles();
    });
    connect(setFisheyeImageAction, &QAction::triggered, this, [this]() {
        const QString selectedPath = QFileDialog::getOpenFileName(
            this,
            tr("Select Fisheye Image"),
            m_view->fisheyeImagePath().isEmpty() ? QDir::currentPath() : QFileInfo(m_view->fisheyeImagePath()).absolutePath(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
        if (selectedPath.isEmpty()) {
            return;
        }
        if (!m_view->setFisheyeImagePath(selectedPath)) {
            return;
        }
        syncInfoPanel();
        saveStateFiles();
    });
    connect(reloadFisheyeImageAction, &QAction::triggered, this, [this]() {
        const QString currentPath = m_view->fisheyeImagePath();
        if (currentPath.isEmpty()) {
            return;
        }
        if (!m_view->setFisheyeImagePath(currentPath)) {
            return;
        }
        syncInfoPanel();
        saveStateFiles();
    });
    connect(clearFisheyeImageAction, &QAction::triggered, this, [this]() {
        m_view->setFisheyeImagePath(QString());
        syncInfoPanel();
        saveStateFiles();
    });

    connect(m_latitudeSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setLatitude(static_cast<float>(value));
        }
    });
    connect(m_longitudeSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setLongitude(static_cast<float>(value));
        }
    });
    connect(m_rollSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setRoll(static_cast<float>(value));
        }
    });
    connect(m_zoomSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setZoom(static_cast<float>(value));
        }
    });
    connect(m_capPolarAngleSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setCapPolarAngle(static_cast<float>(value));
        }
    });
    connect(m_globeRadiusSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setGlobeRadius(static_cast<float>(value));
        }
    });
    auto connectGlobeOrientationSpin = [this](QDoubleSpinBox *) {
        return [this](double) {
            if (m_syncingInfoPanel) {
                return;
            }

            const QQuaternion orientation(
                static_cast<float>(m_globeOrientationWSpin->value()),
                static_cast<float>(m_globeOrientationXSpin->value()),
                static_cast<float>(m_globeOrientationYSpin->value()),
                static_cast<float>(m_globeOrientationZSpin->value()));
            m_view->setGlobeOrientation(orientation);
        };
    };
    connect(m_globeOrientationWSpin, &QDoubleSpinBox::valueChanged, this, connectGlobeOrientationSpin(m_globeOrientationWSpin));
    connect(m_globeOrientationXSpin, &QDoubleSpinBox::valueChanged, this, connectGlobeOrientationSpin(m_globeOrientationXSpin));
    connect(m_globeOrientationYSpin, &QDoubleSpinBox::valueChanged, this, connectGlobeOrientationSpin(m_globeOrientationYSpin));
    connect(m_globeOrientationZSpin, &QDoubleSpinBox::valueChanged, this, connectGlobeOrientationSpin(m_globeOrientationZSpin));
    connect(m_cameraEyeOffsetXSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraEyeOffsetX(static_cast<float>(value));
        }
    });
    connect(m_cameraEyeOffsetYSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraEyeOffsetY(static_cast<float>(value));
        }
    });
    connect(m_cameraEyeOffsetZSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraEyeOffsetZ(static_cast<float>(value));
        }
    });
    connect(m_cameraUpXSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraUpX(static_cast<float>(value));
        }
    });
    connect(m_cameraUpYSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraUpY(static_cast<float>(value));
        }
    });
    connect(m_cameraUpZSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraUpZ(static_cast<float>(value));
        }
    });
    connect(m_latitudeLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setLatitudeLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_longitudeLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setLongitudeLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_rollLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setRollLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_zoomLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setZoomLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_capPolarAngleLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCapPolarAngleLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_globeRadiusLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setGlobeRadiusLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_globeOrientationWLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setGlobeOrientationScalarLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_globeOrientationXLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setGlobeOrientationXLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_globeOrientationYLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setGlobeOrientationYLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_globeOrientationZLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setGlobeOrientationZLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_cameraEyeOffsetXLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraEyeOffsetXLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_cameraEyeOffsetYLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraEyeOffsetYLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_cameraEyeOffsetZLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraEyeOffsetZLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_cameraUpXLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraUpXLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_cameraUpYLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraUpYLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_cameraUpZLock, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCameraUpZLocked(checked);
            syncInfoPanel();
        }
    });
    connect(m_currentHeaderButton, &QPushButton::clicked, this, [this]() {
        if (m_syncingInfoPanel) {
            return;
        }
        if (setShowingCurrentState(true, true)) {
            saveStateFiles();
        }
    });
    connect(m_defaultHeaderButton, &QPushButton::clicked, this, [this]() {
        if (m_syncingInfoPanel) {
            return;
        }
        if (setShowingCurrentState(false, true)) {
            saveStateFiles();
        }
    });
    connect(m_centerButton, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setGlobalCenterFixed(checked);
        }
    });
    connect(m_yTraceButton, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setPanoramaBandVisible(checked);
        }
    });
    connect(m_xCylinderButton, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setXTangentCylinderVisible(checked);
        }
    });
    connect(m_surfaceDepthButton, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setSurfaceDepthHidingEnabled(checked);
        }
    });
    connect(m_capClosedButton, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setCapClosed(checked);
        }
    });
    connect(m_projectionButton, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_syncingInfoPanel) {
            m_view->setPerspectiveProjection(checked);
        }
    });
    connect(m_perspectiveFovYSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!m_syncingInfoPanel) {
            m_view->setPerspectiveFovY(static_cast<float>(value));
        }
    });

    auto connectComponentSpin = [this](QDoubleSpinBox *spin, FrameMath::ComponentId id) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, id](double value) {
            if (!m_syncingInfoPanel) {
                m_view->setComponentLength(id, static_cast<float>(value));
            }
        });
    };
    connectComponentSpin(m_positiveXSpin, FrameMath::ComponentId::PositiveX);
    connectComponentSpin(m_negtiveXSpin, FrameMath::ComponentId::NegtiveX);
    connectComponentSpin(m_positiveYSpin, FrameMath::ComponentId::PositiveY);
    connectComponentSpin(m_negtiveYSpin, FrameMath::ComponentId::NegtiveY);
    connectComponentSpin(m_positiveZSpin, FrameMath::ComponentId::PositiveZ);
    auto connectComponentLock = [this](QCheckBox *lock, FrameMath::ComponentId id) {
        connect(lock, &QCheckBox::toggled, this, [this, id](bool checked) {
            if (!m_syncingInfoPanel) {
                m_view->setComponentLocked(id, checked);
                syncInfoPanel();
            }
        });
    };
    connectComponentLock(m_positiveXLock, FrameMath::ComponentId::PositiveX);
    connectComponentLock(m_negtiveXLock, FrameMath::ComponentId::NegtiveX);
    connectComponentLock(m_positiveYLock, FrameMath::ComponentId::PositiveY);
    connectComponentLock(m_negtiveYLock, FrameMath::ComponentId::NegtiveY);
    connectComponentLock(m_positiveZLock, FrameMath::ComponentId::PositiveZ);

    loadStartupSettings();
    syncInfoPanel();
    configureStateFileWatcher();

    setWindowTitle(tr("Pixel To Ray Qt Window"));
    QTimer::singleShot(0, this, [this]() {
        showWindowOnPreferredScreen(this);
    });
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveStateFiles();
    QMainWindow::closeEvent(event);
}

void MainWindow::installUiFontShortcuts() {
    auto addShortcut = [this](const QKeySequence &sequence, const auto &handler) {
        auto *shortcut = new QShortcut(sequence, this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, handler);
    };

    addShortcut(QKeySequence(QStringLiteral("Ctrl++")), [this]() {
        adjustUiFontScale(kUiFontScaleStep);
    });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+=")), [this]() {
        adjustUiFontScale(kUiFontScaleStep);
    });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+-")), [this]() {
        adjustUiFontScale(-kUiFontScaleStep);
    });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+0")), [this]() {
        resetUiFontScale();
    });
}

void MainWindow::adjustUiFontScale(double delta) {
    const double nextScale = std::clamp(m_uiFontScale + delta, kMinUiFontScale, kMaxUiFontScale);
    if (std::abs(nextScale - m_uiFontScale) < 1e-6) {
        return;
    }
    m_uiFontScale = nextScale;
    applyUiFontScale();
    saveActiveProfileRuntimeState();
}

void MainWindow::resetUiFontScale() {
    if (std::abs(m_uiFontScale - 1.0) < 1e-6) {
        return;
    }
    m_uiFontScale = 1.0;
    applyUiFontScale();
    saveActiveProfileRuntimeState();
}

void MainWindow::applyUiFontScale() {
    QFont appFont = QApplication::font();
    appFont.setPointSizeF(std::max(1.0, m_baseUiFontPointSize * m_uiFontScale));
    QApplication::setFont(appFont);
    applyUiFontScale(this);

    if (m_controlOverlayPanel != nullptr) {
        const int minWidth = std::max(360, static_cast<int>(std::lround(kControlOverlayBaseMinWidth * m_uiFontScale)));
        const int maxWidth = std::max(minWidth, static_cast<int>(std::lround(kControlOverlayBaseMaxWidth * m_uiFontScale)));
        m_controlOverlayPanel->setMinimumWidth(minWidth);
        m_controlOverlayPanel->setMaximumWidth(maxWidth);
        if (m_controlOverlayPanel->isVisible()) {
            m_controlOverlayPanel->adjustSize();
        }
    }
}

void MainWindow::applyUiFontScale(QWidget *widget) {
    if (widget == nullptr) {
        return;
    }

    const QList<QWidget *> children = widget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *child : children) {
        applyUiFontScale(child);
    }

    bool hasBasePointSize = false;
    double basePointSize = widget->property(kBaseFontPointSizeProperty).toDouble(&hasBasePointSize);
    if (!hasBasePointSize || basePointSize <= 0.0) {
        basePointSize = widget->font().pointSizeF();
        if (basePointSize <= 0.0) {
            basePointSize = m_baseUiFontPointSize;
        }
        widget->setProperty(kBaseFontPointSizeProperty, basePointSize);
    }

    QFont scaledFont = widget->font();
    scaledFont.setPointSizeF(std::max(1.0, basePointSize * m_uiFontScale));
    widget->setFont(scaledFont);
}

void MainWindow::loadStartupSettings() {
    m_uiFontScale = readUiFontScaleRuntimeState(m_uiFontScale);
    applyUiFontScale();
    m_showingCurrentState = readActiveProfileRuntimeState(true);
    if (m_showingCurrentState) {
        applyPanelState(m_currentStateSnapshot);
        m_currentStateSnapshot = capturePanelState();
    } else {
        applyDefaultPanelState();
        m_defaultStateProfile = capturePanelState();
    }
    saveActiveProfileRuntimeState();
}

MainWindow::PanelState MainWindow::capturePanelState() const {
    PanelState state;
    state.latitude = m_view->latitude();
    state.longitude = m_view->longitude();
    state.roll = m_view->roll();
    state.zoom = m_view->zoom();
    state.capPolarAngle = m_view->capPolarAngle();
    state.globeRadius = m_view->globeRadius();
    state.globeOrientation = m_view->globeOrientation();
    state.fisheyeImagePath = m_view->fisheyeImagePath();
    state.profileComment = m_showingCurrentState
        ? m_currentStateSnapshot.profileComment
        : m_defaultStateProfile.profileComment;
    state.perspectiveFovY = m_view->perspectiveFovY();
    state.cameraEyeOffsetX = m_view->cameraEyeOffsetX();
    state.cameraEyeOffsetY = m_view->cameraEyeOffsetY();
    state.cameraEyeOffsetZ = m_view->cameraEyeOffsetZ();
    state.cameraUpX = m_view->cameraUpX();
    state.cameraUpY = m_view->cameraUpY();
    state.cameraUpZ = m_view->cameraUpZ();
    state.rearHemisphereWireframeVisible = m_view->isRearHemisphereWireframeVisible();
    state.worldAxesVisible = m_view->isWorldAxesVisible();
    state.localCoordinateFrameVisible = m_view->isLocalCoordinateFrameVisible();
    state.globalCenterFixed = m_view->isGlobalCenterFixed();
    state.panoramaBandVisible = m_view->isPanoramaBandVisible();
    state.xTangentCylinderVisible = m_view->isXTangentCylinderVisible();
    state.surfaceDepthHidingEnabled = m_view->isSurfaceDepthHidingEnabled();
    state.capClosed = m_view->isCapClosed();
    state.perspectiveProjection = m_view->isPerspectiveProjection();
    state.positiveX = m_view->componentLength(FrameMath::ComponentId::PositiveX);
    state.negtiveX = m_view->componentLength(FrameMath::ComponentId::NegtiveX);
    state.positiveY = m_view->componentLength(FrameMath::ComponentId::PositiveY);
    state.negtiveY = m_view->componentLength(FrameMath::ComponentId::NegtiveY);
    state.positiveZ = m_view->componentLength(FrameMath::ComponentId::PositiveZ);
    state.latitudeLocked = m_view->isLatitudeLocked();
    state.longitudeLocked = m_view->isLongitudeLocked();
    state.rollLocked = m_view->isRollLocked();
    state.zoomLocked = m_view->isZoomLocked();
    state.capPolarAngleLocked = m_view->isCapPolarAngleLocked();
    state.globeRadiusLocked = m_view->isGlobeRadiusLocked();
    state.globeOrientationScalarLocked = m_view->isGlobeOrientationScalarLocked();
    state.globeOrientationXLocked = m_view->isGlobeOrientationXLocked();
    state.globeOrientationYLocked = m_view->isGlobeOrientationYLocked();
    state.globeOrientationZLocked = m_view->isGlobeOrientationZLocked();
    state.cameraEyeOffsetXLocked = m_view->isCameraEyeOffsetXLocked();
    state.cameraEyeOffsetYLocked = m_view->isCameraEyeOffsetYLocked();
    state.cameraEyeOffsetZLocked = m_view->isCameraEyeOffsetZLocked();
    state.cameraUpXLocked = m_view->isCameraUpXLocked();
    state.cameraUpYLocked = m_view->isCameraUpYLocked();
    state.cameraUpZLocked = m_view->isCameraUpZLocked();
    state.positiveXLocked = m_view->isComponentLocked(FrameMath::ComponentId::PositiveX);
    state.negtiveXLocked = m_view->isComponentLocked(FrameMath::ComponentId::NegtiveX);
    state.positiveYLocked = m_view->isComponentLocked(FrameMath::ComponentId::PositiveY);
    state.negtiveYLocked = m_view->isComponentLocked(FrameMath::ComponentId::NegtiveY);
    state.positiveZLocked = m_view->isComponentLocked(FrameMath::ComponentId::PositiveZ);
    return state;
}

void MainWindow::applyPanelState(const PanelState &state) {
    m_loadingSettings = true;

    m_view->setLatitudeLocked(false);
    m_view->setLongitudeLocked(false);
    m_view->setRollLocked(false);
    m_view->setZoomLocked(false);
    m_view->setCapPolarAngleLocked(false);
    m_view->setGlobeRadiusLocked(false);
    m_view->setGlobeOrientationScalarLocked(false);
    m_view->setGlobeOrientationXLocked(false);
    m_view->setGlobeOrientationYLocked(false);
    m_view->setGlobeOrientationZLocked(false);
    m_view->setCameraEyeOffsetXLocked(false);
    m_view->setCameraEyeOffsetYLocked(false);
    m_view->setCameraEyeOffsetZLocked(false);
    m_view->setCameraUpXLocked(false);
    m_view->setCameraUpYLocked(false);
    m_view->setCameraUpZLocked(false);
    m_view->setComponentLocked(FrameMath::ComponentId::PositiveX, false);
    m_view->setComponentLocked(FrameMath::ComponentId::NegtiveX, false);
    m_view->setComponentLocked(FrameMath::ComponentId::PositiveY, false);
    m_view->setComponentLocked(FrameMath::ComponentId::NegtiveY, false);
    m_view->setComponentLocked(FrameMath::ComponentId::PositiveZ, false);

    m_view->setLatitude(state.latitude);
    m_view->setLongitude(state.longitude);
    m_view->setRoll(state.roll);
    m_view->setZoom(state.zoom);
    m_view->setCapPolarAngle(state.capPolarAngle);
    m_view->setGlobeRadius(state.globeRadius);
    m_view->setGlobeOrientation(state.globeOrientation);
    m_view->setFisheyeImagePath(resolveFisheyeImagePath(state.fisheyeImagePath));
    m_view->setPerspectiveFovY(state.perspectiveFovY);
    m_view->setCameraEyeOffsetX(state.cameraEyeOffsetX);
    m_view->setCameraEyeOffsetY(state.cameraEyeOffsetY);
    m_view->setCameraEyeOffsetZ(state.cameraEyeOffsetZ);
    m_view->setCameraUpX(state.cameraUpX);
    m_view->setCameraUpY(state.cameraUpY);
    m_view->setCameraUpZ(state.cameraUpZ);
    m_view->setRearHemisphereWireframeVisible(state.rearHemisphereWireframeVisible);
    m_view->setWorldAxesVisible(state.worldAxesVisible);
    m_view->setLocalCoordinateFrameVisible(state.localCoordinateFrameVisible);
    m_view->setGlobalCenterFixed(state.globalCenterFixed);
    m_view->setPanoramaBandVisible(state.panoramaBandVisible);
    m_view->setXTangentCylinderVisible(state.xTangentCylinderVisible);
    m_view->setSurfaceDepthHidingEnabled(state.surfaceDepthHidingEnabled);
    m_view->setCapClosed(state.capClosed);
    m_view->setPerspectiveProjection(state.perspectiveProjection);
    m_view->setComponentLength(FrameMath::ComponentId::PositiveX, state.positiveX);
    m_view->setComponentLength(FrameMath::ComponentId::NegtiveX, state.negtiveX);
    m_view->setComponentLength(FrameMath::ComponentId::PositiveY, state.positiveY);
    m_view->setComponentLength(FrameMath::ComponentId::NegtiveY, state.negtiveY);
    m_view->setComponentLength(FrameMath::ComponentId::PositiveZ, state.positiveZ);

    m_view->setLatitudeLocked(state.latitudeLocked);
    m_view->setLongitudeLocked(state.longitudeLocked);
    m_view->setRollLocked(state.rollLocked);
    m_view->setZoomLocked(state.zoomLocked);
    m_view->setCapPolarAngleLocked(state.capPolarAngleLocked);
    m_view->setGlobeRadiusLocked(state.globeRadiusLocked);
    m_view->setGlobeOrientationScalarLocked(state.globeOrientationScalarLocked);
    m_view->setGlobeOrientationXLocked(state.globeOrientationXLocked);
    m_view->setGlobeOrientationYLocked(state.globeOrientationYLocked);
    m_view->setGlobeOrientationZLocked(state.globeOrientationZLocked);
    m_view->setCameraEyeOffsetXLocked(state.cameraEyeOffsetXLocked);
    m_view->setCameraEyeOffsetYLocked(state.cameraEyeOffsetYLocked);
    m_view->setCameraEyeOffsetZLocked(state.cameraEyeOffsetZLocked);
    m_view->setCameraUpXLocked(state.cameraUpXLocked);
    m_view->setCameraUpYLocked(state.cameraUpYLocked);
    m_view->setCameraUpZLocked(state.cameraUpZLocked);
    m_view->setComponentLocked(FrameMath::ComponentId::PositiveX, state.positiveXLocked);
    m_view->setComponentLocked(FrameMath::ComponentId::NegtiveX, state.negtiveXLocked);
    m_view->setComponentLocked(FrameMath::ComponentId::PositiveY, state.positiveYLocked);
    m_view->setComponentLocked(FrameMath::ComponentId::NegtiveY, state.negtiveYLocked);
    m_view->setComponentLocked(FrameMath::ComponentId::PositiveZ, state.positiveZLocked);

    m_loadingSettings = false;
}

void MainWindow::applyDefaultPanelState() {
    applyPanelState(m_defaultStateProfile);
}

void MainWindow::applySavedPanelState() {
    const PanelState state = loadPanelStateFromJson(currentStateJsonPath(), m_defaultStateProfile);
    applyPanelState(state);
}

void MainWindow::saveStateFiles() {
    if (m_loadingSettings) {
        return;
    }

    if (m_showingCurrentState) {
        m_currentStateSnapshot = capturePanelState();
    } else {
        m_defaultStateProfile = capturePanelState();
    }

    savePanelStateToJson(currentStateJsonPath(), m_currentStateSnapshot);
    savePanelStateToJson(defaultStateJsonPath(), m_defaultStateProfile);
}

bool MainWindow::setShowingCurrentState(bool showCurrent, bool persistRuntimeState) {
    if (showCurrent == m_showingCurrentState) {
        if (persistRuntimeState) {
            saveActiveProfileRuntimeState();
        }
        return false;
    }

    if (!showCurrent) {
        m_currentStateSnapshot = capturePanelState();
        m_showingCurrentState = false;
        applyDefaultPanelState();
    } else {
        m_showingCurrentState = true;
        applyPanelState(m_currentStateSnapshot);
    }

    syncInfoPanel();
    if (persistRuntimeState) {
        saveActiveProfileRuntimeState();
    }
    return true;
}

void MainWindow::configureStateFileWatcher() {
    if (m_stateFileWatcher == nullptr) {
        m_stateFileWatcher = new QFileSystemWatcher(this);
    }

    const auto watchFile = [this](const QString &path) {
        if (QFileInfo::exists(path) && !m_stateFileWatcher->files().contains(path)) {
            m_stateFileWatcher->addPath(path);
        }
    };
    watchFile(currentStateJsonPath());
    watchFile(defaultStateJsonPath());
    watchFile(activeProfileRuntimeJsonPath());

    connect(m_stateFileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
        QTimer::singleShot(120, this, [this]() {
            reloadExternalStateFiles();
        });
    });
}

void MainWindow::reloadExternalStateFiles() {
    if (m_stateFileWatcher != nullptr) {
        const auto watchFile = [this](const QString &path) {
            if (QFileInfo::exists(path) && !m_stateFileWatcher->files().contains(path)) {
                m_stateFileWatcher->addPath(path);
            }
        };
        watchFile(currentStateJsonPath());
        watchFile(defaultStateJsonPath());
        watchFile(activeProfileRuntimeJsonPath());
    }

    m_defaultStateProfile = loadPanelStateFromJson(defaultStateJsonPath(), m_defaultStateProfile);
    m_currentStateSnapshot = loadPanelStateFromJson(currentStateJsonPath(), m_defaultStateProfile);
    m_showingCurrentState = readActiveProfileRuntimeState(m_showingCurrentState);
    const double runtimeFontScale = readUiFontScaleRuntimeState(m_uiFontScale);
    if (std::abs(runtimeFontScale - m_uiFontScale) > 1e-6) {
        m_uiFontScale = runtimeFontScale;
        applyUiFontScale();
    }

    if (m_showingCurrentState) {
        applyPanelState(m_currentStateSnapshot);
    } else {
        applyPanelState(m_defaultStateProfile);
    }
    syncInfoPanel();
}

bool MainWindow::readActiveProfileRuntimeState(bool fallbackShowCurrent) const {
    QFile file(activeProfileRuntimeJsonPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return fallbackShowCurrent;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return fallbackShowCurrent;
    }

    const QString fallbackProfile = QString::fromLatin1(
        fallbackShowCurrent ? kCurrentProfileName : kDefaultProfileName);
    const QString activeProfile = document.object()
                                      .value(QLatin1String(kActiveProfileKey))
                                      .toString(fallbackProfile);
    if (activeProfile == QLatin1String(kDefaultProfileName)
        || activeProfile == QLatin1String(kLegacyDefaultProfileName)) {
        return false;
    }
    if (activeProfile == QLatin1String(kCurrentProfileName)) {
        return true;
    }
    return fallbackShowCurrent;
}

double MainWindow::readUiFontScaleRuntimeState(double fallbackScale) const {
    QFile file(activeProfileRuntimeJsonPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return std::clamp(fallbackScale, kMinUiFontScale, kMaxUiFontScale);
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return std::clamp(fallbackScale, kMinUiFontScale, kMaxUiFontScale);
    }

    const double value = document.object()
                             .value(QLatin1String(kUiFontScaleKey))
                             .toDouble(fallbackScale);
    if (!std::isfinite(value)) {
        return std::clamp(fallbackScale, kMinUiFontScale, kMaxUiFontScale);
    }
    return std::clamp(value, kMinUiFontScale, kMaxUiFontScale);
}

void MainWindow::saveActiveProfileRuntimeState() const {
    const QString path = activeProfileRuntimeJsonPath();
    QDir dir(QFileInfo(path).absolutePath());
    dir.mkpath(QStringLiteral("."));

    QJsonObject json;
    QFile existingFile(path);
    if (existingFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument existingDocument = QJsonDocument::fromJson(existingFile.readAll());
        if (existingDocument.isObject()) {
            json = existingDocument.object();
        }
        existingFile.close();
    }

    json.insert(QLatin1String(kActiveProfileKey),
                QString::fromLatin1(m_showingCurrentState ? kCurrentProfileName : kDefaultProfileName));
    json.insert(QLatin1String(kUiFontScaleKey), panelNumber(m_uiFontScale));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    file.close();
}

QString MainWindow::stateProfilesDirectory() const {
    return resolveAssetsDirectoryPath();
}

QString MainWindow::defaultStateJsonPath() const {
    return QDir(stateProfilesDirectory()).filePath(QLatin1String(kPresetStateFileName));
}

QString MainWindow::currentStateJsonPath() const {
    return QDir(stateProfilesDirectory()).filePath(QLatin1String(kCurrentStateFileName));
}

QString MainWindow::activeProfileRuntimeJsonPath() const {
    return QDir(stateProfilesDirectory()).filePath(QLatin1String(kRuntimeStateFileName));
}

MainWindow::PanelState MainWindow::loadPanelStateFromJson(const QString &path,
                                                          const PanelState &fallback) const {
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return fallback;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        return fallback;
    }

    const QJsonObject json = document.object();
    PanelState state = fallback;

    auto readFloat = [&json](const char *key, float fallbackValue) {
        return static_cast<float>(json.value(QLatin1String(key)).toDouble(fallbackValue));
    };
    auto readBool = [&json](const char *key, bool fallbackValue) {
        return json.value(QLatin1String(key)).toBool(fallbackValue);
    };

    state.latitude = readFloat("latitude", state.latitude);
    state.longitude = readFloat("longitude", state.longitude);
    state.roll = readFloat("roll", state.roll);
    state.zoom = readFloat("zoom", state.zoom);
    state.capPolarAngle = readFloat("cap_polar_angle", state.capPolarAngle);
    state.globeRadius = readFloat("globe_radius", state.globeRadius);
    state.globeOrientation = QQuaternion(
        readFloat("globe_orientation_scalar", state.globeOrientation.scalar()),
        readFloat("globe_orientation_x", state.globeOrientation.x()),
        readFloat("globe_orientation_y", state.globeOrientation.y()),
        readFloat("globe_orientation_z", state.globeOrientation.z()));
    state.fisheyeImagePath = hfcRelativeImagePath(
        json.value("fisheye_image_path").toString(state.fisheyeImagePath));
    state.profileComment = json.value(QLatin1String(kProfileCommentKey)).toString(state.profileComment);
    state.perspectiveFovY = readFloat("perspective_fov_y", state.perspectiveFovY);
    state.cameraEyeOffsetX = readFloat("camera_eye_offset_x", state.cameraEyeOffsetX);
    state.cameraEyeOffsetY = readFloat("camera_eye_offset_y", state.cameraEyeOffsetY);
    state.cameraEyeOffsetZ = readFloat("camera_eye_offset_z", state.cameraEyeOffsetZ);
    state.cameraUpX = readFloat("camera_up_x", state.cameraUpX);
    state.cameraUpY = readFloat("camera_up_y", state.cameraUpY);
    state.cameraUpZ = readFloat("camera_up_z", state.cameraUpZ);
    state.rearHemisphereWireframeVisible = readBool("rear_hemisphere_wireframe_visible", state.rearHemisphereWireframeVisible);
    state.worldAxesVisible = readBool("world_axes_visible", state.worldAxesVisible);
    state.localCoordinateFrameVisible = readBool("local_coordinate_frame_visible", state.localCoordinateFrameVisible);
    state.globalCenterFixed = readBool("global_center_fixed", state.globalCenterFixed);
    state.panoramaBandVisible = readBool("panorama_band_visible", state.panoramaBandVisible);
    state.xTangentCylinderVisible = readBool("x_tangent_cylinder_visible", state.xTangentCylinderVisible);
    state.surfaceDepthHidingEnabled = readBool("surface_depth_hiding_enabled", state.surfaceDepthHidingEnabled);
    state.capClosed = readBool("cap_closed", state.capClosed);
    state.perspectiveProjection = readBool("perspective_projection", state.perspectiveProjection);
    state.positiveX = readFloat("positive_x", state.positiveX);
    state.negtiveX = readFloat("negtive_x", state.negtiveX);
    state.positiveY = readFloat("positive_y", state.positiveY);
    state.negtiveY = readFloat("negtive_y", state.negtiveY);
    state.positiveZ = readFloat("positive_z", state.positiveZ);
    state.latitudeLocked = readBool("latitude_locked", state.latitudeLocked);
    state.longitudeLocked = readBool("longitude_locked", state.longitudeLocked);
    state.rollLocked = readBool("roll_locked", state.rollLocked);
    state.zoomLocked = readBool("zoom_locked", state.zoomLocked);
    state.capPolarAngleLocked = readBool("cap_polar_angle_locked", state.capPolarAngleLocked);
    state.globeRadiusLocked = readBool("globe_radius_locked", state.globeRadiusLocked);
    state.globeOrientationScalarLocked = readBool("globe_orientation_scalar_locked", state.globeOrientationScalarLocked);
    state.globeOrientationXLocked = readBool("globe_orientation_x_locked", state.globeOrientationXLocked);
    state.globeOrientationYLocked = readBool("globe_orientation_y_locked", state.globeOrientationYLocked);
    state.globeOrientationZLocked = readBool("globe_orientation_z_locked", state.globeOrientationZLocked);
    state.cameraEyeOffsetXLocked = readBool("camera_eye_offset_x_locked", state.cameraEyeOffsetXLocked);
    state.cameraEyeOffsetYLocked = readBool("camera_eye_offset_y_locked", state.cameraEyeOffsetYLocked);
    state.cameraEyeOffsetZLocked = readBool("camera_eye_offset_z_locked", state.cameraEyeOffsetZLocked);
    state.cameraUpXLocked = readBool("camera_up_x_locked", state.cameraUpXLocked);
    state.cameraUpYLocked = readBool("camera_up_y_locked", state.cameraUpYLocked);
    state.cameraUpZLocked = readBool("camera_up_z_locked", state.cameraUpZLocked);
    state.positiveXLocked = readBool("positive_x_locked", state.positiveXLocked);
    state.negtiveXLocked = readBool("negtive_x_locked", state.negtiveXLocked);
    state.positiveYLocked = readBool("positive_y_locked", state.positiveYLocked);
    state.negtiveYLocked = readBool("negtive_y_locked", state.negtiveYLocked);
    state.positiveZLocked = readBool("positive_z_locked", state.positiveZLocked);
    return state;
}

void MainWindow::savePanelStateToJson(const QString &path, const PanelState &state) const {
    QDir dir(QFileInfo(path).absolutePath());
    dir.mkpath(QStringLiteral("."));

    QJsonObject json;
    auto insertNumber = [&json](const char *key, double value) {
        json.insert(QLatin1String(key), panelNumber(value));
    };
    insertNumber("latitude", state.latitude);
    insertNumber("longitude", state.longitude);
    insertNumber("roll", state.roll);
    insertNumber("zoom", state.zoom);
    insertNumber("cap_polar_angle", state.capPolarAngle);
    insertNumber("globe_radius", state.globeRadius);
    insertNumber("globe_orientation_scalar", state.globeOrientation.scalar());
    insertNumber("globe_orientation_x", state.globeOrientation.x());
    insertNumber("globe_orientation_y", state.globeOrientation.y());
    insertNumber("globe_orientation_z", state.globeOrientation.z());
    json.insert("fisheye_image_path", hfcRelativeImagePath(state.fisheyeImagePath));
    json.insert(QLatin1String(kProfileCommentKey), state.profileComment);
    insertNumber("perspective_fov_y", state.perspectiveFovY);
    insertNumber("camera_eye_offset_x", state.cameraEyeOffsetX);
    insertNumber("camera_eye_offset_y", state.cameraEyeOffsetY);
    insertNumber("camera_eye_offset_z", state.cameraEyeOffsetZ);
    insertNumber("camera_up_x", state.cameraUpX);
    insertNumber("camera_up_y", state.cameraUpY);
    insertNumber("camera_up_z", state.cameraUpZ);
    json.insert("rear_hemisphere_wireframe_visible", state.rearHemisphereWireframeVisible);
    json.insert("world_axes_visible", state.worldAxesVisible);
    json.insert("local_coordinate_frame_visible", state.localCoordinateFrameVisible);
    json.insert("global_center_fixed", state.globalCenterFixed);
    json.insert("panorama_band_visible", state.panoramaBandVisible);
    json.insert("x_tangent_cylinder_visible", state.xTangentCylinderVisible);
    json.insert("surface_depth_hiding_enabled", state.surfaceDepthHidingEnabled);
    json.insert("cap_closed", state.capClosed);
    json.insert("perspective_projection", state.perspectiveProjection);
    insertNumber("positive_x", state.positiveX);
    insertNumber("negtive_x", state.negtiveX);
    insertNumber("positive_y", state.positiveY);
    insertNumber("negtive_y", state.negtiveY);
    insertNumber("positive_z", state.positiveZ);
    json.insert("latitude_locked", state.latitudeLocked);
    json.insert("longitude_locked", state.longitudeLocked);
    json.insert("roll_locked", state.rollLocked);
    json.insert("zoom_locked", state.zoomLocked);
    json.insert("cap_polar_angle_locked", state.capPolarAngleLocked);
    json.insert("globe_radius_locked", state.globeRadiusLocked);
    json.insert("globe_orientation_scalar_locked", state.globeOrientationScalarLocked);
    json.insert("globe_orientation_x_locked", state.globeOrientationXLocked);
    json.insert("globe_orientation_y_locked", state.globeOrientationYLocked);
    json.insert("globe_orientation_z_locked", state.globeOrientationZLocked);
    json.insert("camera_eye_offset_x_locked", state.cameraEyeOffsetXLocked);
    json.insert("camera_eye_offset_y_locked", state.cameraEyeOffsetYLocked);
    json.insert("camera_eye_offset_z_locked", state.cameraEyeOffsetZLocked);
    json.insert("camera_up_x_locked", state.cameraUpXLocked);
    json.insert("camera_up_y_locked", state.cameraUpYLocked);
    json.insert("camera_up_z_locked", state.cameraUpZLocked);
    json.insert("positive_x_locked", state.positiveXLocked);
    json.insert("negtive_x_locked", state.negtiveXLocked);
    json.insert("positive_y_locked", state.positiveYLocked);
    json.insert("negtive_y_locked", state.negtiveYLocked);
    json.insert("positive_z_locked", state.positiveZLocked);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    file.close();
}

QDoubleSpinBox *MainWindow::createSpinBox(double minValue, double maxValue, double step, int decimals) {
    auto *spin = new QDoubleSpinBox(this);
    spin->setRange(minValue, maxValue);
    spin->setDecimals(panelDecimals(decimals));
    spin->setSingleStep(step);
    spin->setKeyboardTracking(true);
    spin->setAccelerated(true);
    spin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    spin->setStyleSheet(QStringLiteral(
        "QDoubleSpinBox { background: #1f2937; color: #f9fafb; border: 1px solid #374151; padding: 4px 6px; }"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 18px; }"
    ));
    return spin;
}

QWidget *MainWindow::createLockableEditor(QDoubleSpinBox *spin, QCheckBox **lockCheck) {
    auto *container = new QWidget(this);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(spin, 1);

    auto *check = new QCheckBox(tr("Lock"), container);
    check->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    layout->addWidget(check, 0);
    *lockCheck = check;
    return container;
}

void MainWindow::syncInfoPanel() {
    m_syncingInfoPanel = true;

    const QSignalBlocker b0(m_latitudeSpin);
    const QSignalBlocker b1(m_longitudeSpin);
    const QSignalBlocker b2(m_rollSpin);
    const QSignalBlocker b3(m_zoomSpin);
    const QSignalBlocker b4(m_capPolarAngleSpin);
    const QSignalBlocker b5(m_globeRadiusSpin);
    const QSignalBlocker b6(m_globeOrientationWSpin);
    const QSignalBlocker b7(m_globeOrientationXSpin);
    const QSignalBlocker b8(m_globeOrientationYSpin);
    const QSignalBlocker b9(m_globeOrientationZSpin);
    const QSignalBlocker b10(m_eulerAxisXSpin);
    const QSignalBlocker b11(m_eulerAxisYSpin);
    const QSignalBlocker b12(m_eulerAxisZSpin);
    const QSignalBlocker b13(m_eulerAngleSpin);
    const QSignalBlocker b14(m_perspectiveFovYSpin);
    const QSignalBlocker b15(m_cameraEyeOffsetXSpin);
    const QSignalBlocker b16(m_cameraEyeOffsetYSpin);
    const QSignalBlocker b17(m_cameraEyeOffsetZSpin);
    const QSignalBlocker b18(m_cameraTargetXSpin);
    const QSignalBlocker b19(m_cameraTargetYSpin);
    const QSignalBlocker b20(m_cameraTargetZSpin);
    const QSignalBlocker b21(m_cameraUpXSpin);
    const QSignalBlocker b22(m_cameraUpYSpin);
    const QSignalBlocker b23(m_cameraUpZSpin);
    const QSignalBlocker b24(m_anchorXSpin);
    const QSignalBlocker b25(m_anchorYSpin);
    const QSignalBlocker b26(m_anchorZSpin);
    const QSignalBlocker b27(m_positiveXSpin);
    const QSignalBlocker b28(m_negtiveXSpin);
    const QSignalBlocker b29(m_positiveYSpin);
    const QSignalBlocker b30(m_negtiveYSpin);
    const QSignalBlocker b31(m_positiveZSpin);
    const QSignalBlocker b32(m_negtiveZSpin);
    const QSignalBlocker b33(m_vaSpin);
    const QSignalBlocker b34(m_latitudeLock);
    const QSignalBlocker b35(m_longitudeLock);
    const QSignalBlocker b36(m_rollLock);
    const QSignalBlocker b37(m_zoomLock);
    const QSignalBlocker b38(m_capPolarAngleLock);
    const QSignalBlocker b39(m_globeRadiusLock);
    const QSignalBlocker globeOrientationWLockBlocker(m_globeOrientationWLock);
    const QSignalBlocker globeOrientationXLockBlocker(m_globeOrientationXLock);
    const QSignalBlocker globeOrientationYLockBlocker(m_globeOrientationYLock);
    const QSignalBlocker globeOrientationZLockBlocker(m_globeOrientationZLock);
    const QSignalBlocker b40(m_cameraEyeOffsetXLock);
    const QSignalBlocker b41(m_cameraEyeOffsetYLock);
    const QSignalBlocker b42(m_cameraEyeOffsetZLock);
    const QSignalBlocker b43(m_cameraUpXLock);
    const QSignalBlocker b44(m_cameraUpYLock);
    const QSignalBlocker b45(m_cameraUpZLock);
    const QSignalBlocker b46(m_positiveXLock);
    const QSignalBlocker b47(m_negtiveXLock);
    const QSignalBlocker b48(m_positiveYLock);
    const QSignalBlocker b49(m_negtiveYLock);
    const QSignalBlocker b50(m_positiveZLock);
    const QSignalBlocker b51(m_currentHeaderButton);
    const QSignalBlocker b52(m_defaultHeaderButton);
    const QSignalBlocker b53(m_centerButton);
    const QSignalBlocker b54(m_yTraceButton);
    const QSignalBlocker b55(m_xCylinderButton);
    const QSignalBlocker b56(m_surfaceDepthButton);
    const QSignalBlocker b57(m_capClosedButton);
    const QSignalBlocker b58(m_projectionButton);

    m_activeLabel->setText(tr("%1  [x/X y/Y z/Z]").arg(m_view->activeComponentKey()));
    if (m_view->fisheyeImagePath().isEmpty()) {
        m_fisheyeImageLabel->setText(tr("none"));
        m_fisheyeImageLabel->setToolTip(QString());
    } else {
        const QFileInfo info(m_view->fisheyeImagePath());
        m_fisheyeImageLabel->setText(info.fileName());
        m_fisheyeImageLabel->setToolTip(m_view->fisheyeImagePath());
    }
    m_currentHeaderButton->setChecked(m_showingCurrentState);
    m_defaultHeaderButton->setChecked(!m_showingCurrentState);
    m_centerButton->setChecked(m_view->isGlobalCenterFixed());
    m_centerButton->setText(m_view->isGlobalCenterFixed()
                                ? tr("fixed  [toggle here or right click menu]")
                                : tr("move  [toggle here or right click menu]"));
    m_yTraceButton->setChecked(m_view->isPanoramaBandVisible());
    m_yTraceButton->setText(m_view->isPanoramaBandVisible()
                                ? tr("on  [toggle here or right click menu]")
                                : tr("off  [toggle here or right click menu]"));
    m_xCylinderButton->setChecked(m_view->isXTangentCylinderVisible());
    m_xCylinderButton->setText(m_view->isXTangentCylinderVisible()
                                   ? tr("on  [toggle here or right click menu]")
                                   : tr("off  [toggle here or right click menu]"));
    m_surfaceDepthButton->setChecked(m_view->isSurfaceDepthHidingEnabled());
    m_surfaceDepthButton->setText(m_view->isSurfaceDepthHidingEnabled()
                                      ? tr("on  [toggle here or right click menu]")
                                      : tr("off  [toggle here or right click menu]"));
    m_capClosedButton->setChecked(m_view->isCapClosed());
    m_capClosedButton->setText(m_view->isCapClosed()
                                   ? tr("on  [toggle here or right click menu]")
                                   : tr("off  [toggle here or right click menu]"));
    m_projectionButton->setChecked(m_view->isPerspectiveProjection());
    m_projectionButton->setText(m_view->isPerspectiveProjection()
                                    ? tr("perspective  [toggle here]")
                                    : tr("orthographic  [toggle here]"));

    m_latitudeSpin->setValue(m_view->latitude());
    m_longitudeSpin->setValue(m_view->longitude());
    m_rollSpin->setValue(m_view->roll());
    m_zoomSpin->setValue(m_view->zoom());
    m_capPolarAngleSpin->setValue(m_view->capPolarAngle());
    m_globeRadiusSpin->setValue(m_view->globeRadius());
    m_perspectiveFovYSpin->setValue(m_view->perspectiveFovY());
    m_globeOrientationWSpin->setValue(m_view->globeOrientation().scalar());
    m_globeOrientationXSpin->setValue(m_view->globeOrientation().x());
    m_globeOrientationYSpin->setValue(m_view->globeOrientation().y());
    m_globeOrientationZSpin->setValue(m_view->globeOrientation().z());
    const AxisAngleComponents axisAngle = axisAngleFromQuaternion(m_view->globeOrientation());
    m_eulerAxisXSpin->setValue(axisAngle.axisX);
    m_eulerAxisYSpin->setValue(axisAngle.axisY);
    m_eulerAxisZSpin->setValue(axisAngle.axisZ);
    m_eulerAngleSpin->setValue(axisAngle.angleDegrees);
    m_cameraEyeOffsetXSpin->setValue(m_view->cameraEyeOffsetX());
    m_cameraEyeOffsetYSpin->setValue(m_view->cameraEyeOffsetY());
    m_cameraEyeOffsetZSpin->setValue(m_view->cameraEyeOffsetZ());
    m_cameraTargetXSpin->setValue(m_view->cameraTargetX());
    m_cameraTargetYSpin->setValue(m_view->cameraTargetY());
    m_cameraTargetZSpin->setValue(m_view->cameraTargetZ());
    m_cameraUpXSpin->setValue(m_view->cameraUpX());
    m_cameraUpYSpin->setValue(m_view->cameraUpY());
    m_cameraUpZSpin->setValue(m_view->cameraUpZ());
    m_negtiveZSpin->setRange(m_view->globeRadius(), m_view->globeRadius());
    m_anchorXSpin->setValue(m_view->anchorGlobalX());
    m_anchorYSpin->setValue(m_view->anchorGlobalY());
    m_anchorZSpin->setValue(m_view->anchorGlobalZ());
    m_positiveXSpin->setValue(m_view->componentLength(FrameMath::ComponentId::PositiveX));
    m_negtiveXSpin->setValue(m_view->componentLength(FrameMath::ComponentId::NegtiveX));
    m_positiveYSpin->setValue(m_view->componentLength(FrameMath::ComponentId::PositiveY));
    m_vaSpin->setValue(vaAngleDegrees(m_view->componentLength(FrameMath::ComponentId::PositiveY), m_view->globeRadius()));
    m_negtiveYSpin->setValue(m_view->componentLength(FrameMath::ComponentId::NegtiveY));
    m_positiveZSpin->setValue(m_view->componentLength(FrameMath::ComponentId::PositiveZ));
    m_negtiveZSpin->setValue(m_view->componentLength(FrameMath::ComponentId::NegtiveZ));
    m_latitudeLock->setChecked(m_view->isLatitudeLocked());
    m_longitudeLock->setChecked(m_view->isLongitudeLocked());
    m_rollLock->setChecked(m_view->isRollLocked());
    m_zoomLock->setChecked(m_view->isZoomLocked());
    m_capPolarAngleLock->setChecked(m_view->isCapPolarAngleLocked());
    m_globeOrientationWLock->setChecked(m_view->isGlobeOrientationScalarLocked());
    m_globeOrientationXLock->setChecked(m_view->isGlobeOrientationXLocked());
    m_globeOrientationYLock->setChecked(m_view->isGlobeOrientationYLocked());
    m_globeOrientationZLock->setChecked(m_view->isGlobeOrientationZLocked());
    m_cameraEyeOffsetXLock->setChecked(m_view->isCameraEyeOffsetXLocked());
    m_cameraEyeOffsetYLock->setChecked(m_view->isCameraEyeOffsetYLocked());
    m_cameraEyeOffsetZLock->setChecked(m_view->isCameraEyeOffsetZLocked());
    m_cameraUpXLock->setChecked(m_view->isCameraUpXLocked());
    m_cameraUpYLock->setChecked(m_view->isCameraUpYLocked());
    m_cameraUpZLock->setChecked(m_view->isCameraUpZLocked());
    m_positiveXLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::PositiveX));
    m_negtiveXLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::NegtiveX));
    m_positiveYLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::PositiveY));
    m_negtiveYLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::NegtiveY));
    m_positiveZLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::PositiveZ));
    m_globeRadiusLock->setChecked(m_view->isGlobeRadiusLocked());

    m_latitudeSpin->setEnabled(!m_view->isLatitudeLocked());
    m_longitudeSpin->setEnabled(!m_view->isLongitudeLocked());
    m_rollSpin->setEnabled(!m_view->isRollLocked());
    m_zoomSpin->setEnabled(!m_view->isZoomLocked());
    m_capPolarAngleSpin->setEnabled(!m_view->isCapPolarAngleLocked());
    m_globeRadiusSpin->setEnabled(!m_view->isGlobeRadiusLocked());
    m_globeOrientationWSpin->setEnabled(!m_view->isGlobeOrientationScalarLocked());
    m_globeOrientationXSpin->setEnabled(!m_view->isGlobeOrientationXLocked());
    m_globeOrientationYSpin->setEnabled(!m_view->isGlobeOrientationYLocked());
    m_globeOrientationZSpin->setEnabled(!m_view->isGlobeOrientationZLocked());
    m_cameraEyeOffsetXSpin->setEnabled(!m_view->isCameraEyeOffsetXLocked());
    m_cameraEyeOffsetYSpin->setEnabled(!m_view->isCameraEyeOffsetYLocked());
    m_cameraEyeOffsetZSpin->setEnabled(!m_view->isCameraEyeOffsetZLocked());
    m_cameraUpXSpin->setEnabled(!m_view->isCameraUpXLocked());
    m_cameraUpYSpin->setEnabled(!m_view->isCameraUpYLocked());
    m_cameraUpZSpin->setEnabled(!m_view->isCameraUpZLocked());
    m_positiveXSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::PositiveX));
    m_negtiveXSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::NegtiveX));
    m_positiveYSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::PositiveY));
    m_negtiveYSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::NegtiveY));
    m_positiveZSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::PositiveZ));

    m_syncingInfoPanel = false;
}

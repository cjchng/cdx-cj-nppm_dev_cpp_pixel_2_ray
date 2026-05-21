#include "MainWindow.h"

#include "PixelToRayWidget.h"

#include <QCursor>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPalette>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

bool matchesPreferredScreen(QScreen *screen) {
    if (screen == nullptr) {
        return false;
    }

    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    return screen->name().contains(QStringLiteral("EV2785"), cs)
        || screen->model().contains(QStringLiteral("EV2785"), cs)
        || screen->manufacturer().contains(QStringLiteral("EIZO"), cs);
}

QScreen *preferredStartupScreen(QMainWindow *window) {
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *currentScreen = QGuiApplication::screenAt(QCursor::pos());
    if (currentScreen == nullptr) {
        currentScreen = window->screen();
    }

    for (QScreen *screen : screens) {
        if (matchesPreferredScreen(screen)) {
            return screen;
        }
    }

    if (screens.size() > 1) {
        for (QScreen *screen : screens) {
            if (screen != nullptr && screen != currentScreen) {
                return screen;
            }
        }
    }

    if (currentScreen != nullptr) {
        return currentScreen;
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

QWidget *createSectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: #f3f4f6; margin-top: 6px;"));
    return label;
}

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_view = new PixelToRayWidget(central);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *panel = new QWidget(central);
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    panel->setMinimumWidth(360);

    QPalette palette = panel->palette();
    palette.setColor(QPalette::Window, QColor(0x11, 0x18, 0x27));
    palette.setColor(QPalette::WindowText, QColor(0xe5, 0xe7, 0xeb));
    panel->setAutoFillBackground(true);
    panel->setPalette(palette);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(10);

    auto *title = new QLabel(tr("Pixel To Ray Controls"), panel);
    QFont titleFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 4.0);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setStyleSheet(QStringLiteral("color: #f9fafb;"));
    panelLayout->addWidget(title);

    auto *statusForm = new QFormLayout();
    statusForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    statusForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusForm->setFormAlignment(Qt::AlignTop);

    m_activeLabel = new QLabel(panel);
    m_centerLabel = new QLabel(panel);
    m_yTraceLabel = new QLabel(panel);
    m_xCylinderLabel = new QLabel(panel);
    for (QLabel *label : {m_activeLabel, m_centerLabel, m_yTraceLabel, m_xCylinderLabel}) {
        label->setStyleSheet(QStringLiteral("color: #e5e7eb;"));
    }
    statusForm->addRow(tr("Active"), m_activeLabel);
    statusForm->addRow(tr("Center"), m_centerLabel);
    statusForm->addRow(tr("Y Trace"), m_yTraceLabel);
    statusForm->addRow(tr("X Cylinder"), m_xCylinderLabel);
    panelLayout->addWidget(createSectionLabel(tr("Status"), panel));
    panelLayout->addLayout(statusForm);

    auto *viewForm = new QFormLayout();
    viewForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_latitudeSpin = createSpinBox(-89.0, 89.0, 1.0, 2);
    m_longitudeSpin = createSpinBox(-9999.0, 9999.0, 1.0, 2);
    m_rollSpin = createSpinBox(-9999.0, 9999.0, 1.0, 2);
    m_zoomSpin = createSpinBox(0.35, 2.5, 0.05, 2);
    m_capPolarAngleSpin = createSpinBox(10.0, 300.0, 1.0, 1);
    viewForm->addRow(tr("Latitude"), createLockableEditor(m_latitudeSpin, &m_latitudeLock));
    viewForm->addRow(tr("Longitude"), createLockableEditor(m_longitudeSpin, &m_longitudeLock));
    viewForm->addRow(tr("Roll"), createLockableEditor(m_rollSpin, &m_rollLock));
    viewForm->addRow(tr("Zoom"), createLockableEditor(m_zoomSpin, &m_zoomLock));
    viewForm->addRow(tr("Cap Angle"), createLockableEditor(m_capPolarAngleSpin, &m_capPolarAngleLock));
    panelLayout->addWidget(createSectionLabel(tr("View"), panel));
    panelLayout->addLayout(viewForm);

    auto *componentForm = new QFormLayout();
    componentForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_positiveXSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_negtiveXSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_positiveYSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_negtiveYSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_positiveZSpin = createSpinBox(FrameMath::kMinLength, FrameMath::kMaxLength, 0.01, 3);
    m_negtiveZSpin = createSpinBox(FrameMath::kSphereRadius, FrameMath::kSphereRadius, 0.0, 3);
    m_negtiveZSpin->setEnabled(false);
    componentForm->addRow(tr("positive_x"), createLockableEditor(m_positiveXSpin, &m_positiveXLock));
    componentForm->addRow(tr("negtive_x"), createLockableEditor(m_negtiveXSpin, &m_negtiveXLock));
    componentForm->addRow(tr("positive_y"), createLockableEditor(m_positiveYSpin, &m_positiveYLock));
    componentForm->addRow(tr("negtive_y"), createLockableEditor(m_negtiveYSpin, &m_negtiveYLock));
    componentForm->addRow(tr("positive_z"), createLockableEditor(m_positiveZSpin, &m_positiveZLock));
    componentForm->addRow(tr("negtive_z"), m_negtiveZSpin);
    panelLayout->addWidget(createSectionLabel(tr("Components"), panel));
    panelLayout->addLayout(componentForm);
    panelLayout->addStretch(1);

    layout->addWidget(m_view, 3);
    layout->addWidget(panel, 1);
    setCentralWidget(central);

    connect(m_view, &PixelToRayWidget::statusTextChanged, this, [this](const QString &) {
        syncInfoPanel();
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

    syncInfoPanel();

    setWindowTitle(tr("Pixel To Ray Qt Window"));
    placeWindowNearTopRight(this);
    showFullScreen();
}

QDoubleSpinBox *MainWindow::createSpinBox(double minValue, double maxValue, double step, int decimals) {
    auto *spin = new QDoubleSpinBox(this);
    spin->setRange(minValue, maxValue);
    spin->setDecimals(decimals);
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
    const QSignalBlocker b5(m_positiveXSpin);
    const QSignalBlocker b6(m_negtiveXSpin);
    const QSignalBlocker b7(m_positiveYSpin);
    const QSignalBlocker b8(m_negtiveYSpin);
    const QSignalBlocker b9(m_positiveZSpin);
    const QSignalBlocker b10(m_negtiveZSpin);
    const QSignalBlocker b11(m_latitudeLock);
    const QSignalBlocker b12(m_longitudeLock);
    const QSignalBlocker b13(m_rollLock);
    const QSignalBlocker b14(m_zoomLock);
    const QSignalBlocker b15(m_capPolarAngleLock);
    const QSignalBlocker b16(m_positiveXLock);
    const QSignalBlocker b17(m_negtiveXLock);
    const QSignalBlocker b18(m_positiveYLock);
    const QSignalBlocker b19(m_negtiveYLock);
    const QSignalBlocker b20(m_positiveZLock);

    m_activeLabel->setText(m_view->activeComponentKey());
    m_centerLabel->setText(m_view->isGlobalCenterFixed() ? tr("fixed") : tr("move"));
    m_yTraceLabel->setText(m_view->isPanoramaBandVisible() ? tr("on") : tr("off"));
    m_xCylinderLabel->setText(m_view->isXTangentCylinderVisible() ? tr("on") : tr("off"));

    m_latitudeSpin->setValue(m_view->latitude());
    m_longitudeSpin->setValue(m_view->longitude());
    m_rollSpin->setValue(m_view->roll());
    m_zoomSpin->setValue(m_view->zoom());
    m_capPolarAngleSpin->setValue(m_view->capPolarAngle());
    m_positiveXSpin->setValue(m_view->componentLength(FrameMath::ComponentId::PositiveX));
    m_negtiveXSpin->setValue(m_view->componentLength(FrameMath::ComponentId::NegtiveX));
    m_positiveYSpin->setValue(m_view->componentLength(FrameMath::ComponentId::PositiveY));
    m_negtiveYSpin->setValue(m_view->componentLength(FrameMath::ComponentId::NegtiveY));
    m_positiveZSpin->setValue(m_view->componentLength(FrameMath::ComponentId::PositiveZ));
    m_negtiveZSpin->setValue(m_view->componentLength(FrameMath::ComponentId::NegtiveZ));
    m_latitudeLock->setChecked(m_view->isLatitudeLocked());
    m_longitudeLock->setChecked(m_view->isLongitudeLocked());
    m_rollLock->setChecked(m_view->isRollLocked());
    m_zoomLock->setChecked(m_view->isZoomLocked());
    m_capPolarAngleLock->setChecked(m_view->isCapPolarAngleLocked());
    m_positiveXLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::PositiveX));
    m_negtiveXLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::NegtiveX));
    m_positiveYLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::PositiveY));
    m_negtiveYLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::NegtiveY));
    m_positiveZLock->setChecked(m_view->isComponentLocked(FrameMath::ComponentId::PositiveZ));

    m_latitudeSpin->setEnabled(!m_view->isLatitudeLocked());
    m_longitudeSpin->setEnabled(!m_view->isLongitudeLocked());
    m_rollSpin->setEnabled(!m_view->isRollLocked());
    m_zoomSpin->setEnabled(!m_view->isZoomLocked());
    m_capPolarAngleSpin->setEnabled(!m_view->isCapPolarAngleLocked());
    m_positiveXSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::PositiveX));
    m_negtiveXSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::NegtiveX));
    m_positiveYSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::PositiveY));
    m_negtiveYSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::NegtiveY));
    m_positiveZSpin->setEnabled(!m_view->isComponentLocked(FrameMath::ComponentId::PositiveZ));

    m_syncingInfoPanel = false;
}

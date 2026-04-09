#include "MainWindow.h"

#include "PixelToRayWidget.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPalette>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_view = new PixelToRayWidget(central);
    m_infoLabel = new QLabel(central);
    m_infoLabel->setMinimumWidth(290);
    m_infoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_infoLabel->setMargin(12);
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_infoLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    QPalette palette = m_infoLabel->palette();
    palette.setColor(QPalette::Window, QColor(0x11, 0x18, 0x27));
    palette.setColor(QPalette::WindowText, QColor(0xe5, 0xe7, 0xeb));
    m_infoLabel->setAutoFillBackground(true);
    m_infoLabel->setPalette(palette);

    layout->addWidget(m_view, 1);
    layout->addWidget(m_infoLabel);
    setCentralWidget(central);

    connect(m_view, &PixelToRayWidget::statusTextChanged, m_infoLabel, &QLabel::setText);

    setWindowTitle(tr("Pixel To Ray Qt Window"));
    resize(1280, 800);
}

#pragma once

#include <QLabel>
#include <QMainWindow>

class PixelToRayWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    PixelToRayWidget *m_view = nullptr;
    QLabel *m_infoLabel = nullptr;
};

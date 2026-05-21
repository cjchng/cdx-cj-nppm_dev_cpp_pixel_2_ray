#include "FrameMath.h"
#include "MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char *argv[]) {


    FrameMath::runSanityChecks();

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 2); //
    format.setProfile(QSurfaceFormat::CoreProfile); //
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    MainWindow window;
    window.show();

    return app.exec();
}

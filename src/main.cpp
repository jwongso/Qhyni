#include <QApplication>
#include "HyniWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    HyniWindow window;
    window.show();
    return app.exec();
}

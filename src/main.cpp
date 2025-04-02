#include <QApplication>
#include "HyniWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    try {
        HyniWindow window;
        window.show();
        return app.exec();
    } catch (const std::exception& e) {
        qCritical() << "Fatal error:" << e.what();
        return -1;
    }
}

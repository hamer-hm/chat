// client/main.cpp
#include <QApplication>
#include "loginwindow.h"
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("WeChat");

    LoginWindow login;
    if (login.exec() != QDialog::Accepted)
        return 0;

    MainWindow w;
    w.show();
    return app.exec();
}

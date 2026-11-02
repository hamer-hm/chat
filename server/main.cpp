// server/main.cpp
#include <QCoreApplication>
#include "server.h"

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    Server server;
    if (!server.start(8888, "wechat.db"))
        return 1;
    return a.exec();
}

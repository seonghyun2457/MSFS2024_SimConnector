#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SimConnect.h>

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qDebug() << "Hello!";

    return QCoreApplication::exec();
}

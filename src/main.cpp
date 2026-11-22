#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>

#include "simconnector.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    SimConnector simconnector;
    QObject::connect(&simconnector, &SimConnector::connected, [](const QString& iName) { qDebug() << "Ready:" << iName;});
    QObject::connect(&simconnector, &SimConnector::disconnected, [] { qDebug() << "Disconnected";});

    // Print about once per second. The signal itself arrives every sim frame.
    QElapsedTimer printTimer;
    printTimer.start();

    QObject::connect(&simconnector, &SimConnector::flightDataReceived, [&](const FlightData_t& d) {

        if (printTimer.elapsed() < 1000) return;
        printTimer.restart();

        qDebug().noquote() << QString("ALT %1 ft | AoA %2  | CAS %3 kt | M %4 | VS %5 fpm | HDG %6 | PIT %7 | ROL %8 | RA %9 ft")
                                  .arg(d.pressureAltitudeFt, 0, 'f', 0)
                                  .arg(d.angleOfAttackDeg, 0, 'f', 1)
                                  .arg(d.computedAirspeedKt, 0, 'f', 1)
                                  .arg(d.mach, 0, 'f', 3)
                                  .arg(d.verticalSpeedFpm, 0, 'f', 0)
                                  .arg(d.headingMagDeg, 0, 'f', 1)
                                  .arg(d.pitchDeg, 0, 'f', 1)
                                  .arg(d.rollDeg, 0, 'f', 1)
                                  .arg(d.radioHeightFt, 0, 'f', 0);
    });


    simconnector.execute();

    return QCoreApplication::exec();
}

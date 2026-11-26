#include "datarecorder.h"
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>

dataRecorder::dataRecorder(QObject *parent)
    : QObject{parent}
{

}

dataRecorder::~dataRecorder()
{
    stopRecording();
}

void dataRecorder::onFlightData(const FlightData_t& iFlightData)
{
    if (!m_flightDataFile.isOpen()) return;

    m_flightDataStream << m_clock.elapsed();
    for (const FlightDataFile::Column_t& column : FlightDataFile::kColumns) {
        m_flightDataStream << ',' << QString::number(iFlightData.*column.member, 'g', 12);
    }

    m_flightDataStream << '\n';
}

void dataRecorder::onAircraftInfo(const AircraftInfo_t& iAircraftInfo)
{
    if (!m_aircraftInfoFile.isOpen()) return;

    m_aircraftInfoStream << "aircraftInfo"
                         << "cruiseSpeedKt=" << iAircraftInfo.cruiseSpeedKt
                         << "StallSpeedFlapsKt=" << iAircraftInfo.stallSpeedFlapsKt
                         << "stallSpeedCleanKt=" << iAircraftInfo.stallSpeedCleanKt
                         << "engineMaxRpm=" << iAircraftInfo.engineMaxRpm;
}

void dataRecorder::stopRecording()
{
    if (m_flightDataFile.isOpen()) {
        m_flightDataStream.flush();
        m_flightDataFile.close();

        qInfo() << "Closed flight data file " << m_flightDataFile.fileName();
    }

    if (m_aircraftInfoFile.isOpen()) {
        m_aircraftInfoStream.flush();
        m_aircraftInfoFile.close();

        qInfo() << "Closed aircraft info file " << m_aircraftInfoFile.fileName();
    }

    qInfo() << "Stop recording";
}

void dataRecorder::record()
{
    const QString currentTime = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    QDir dir(QCoreApplication::applicationDirPath()+"/flightData");
    if (!dir.mkpath(".")) {
        qWarning() <<"Failed to create a directory " << dir;
    }

    const QString flightDataFileName = QString("flightData_%1.csv").arg(currentTime);
    const QString aircraftInfoFileName = QString("aircraftInfo_%1.csv").arg(currentTime);

    m_flightDataFile.setFileName(dir.filePath(flightDataFileName));
    m_aircraftInfoFile.setFileName(dir.filePath(aircraftInfoFileName));

    // Open file
    if (!m_flightDataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() <<"Failed to open flight data file " << m_flightDataFile.fileName() << m_flightDataFile.errorString();
        return;
    }

    qInfo() << "Opened flight data file " << m_flightDataFile.fileName();
    m_flightDataStream.setDevice(&m_flightDataFile);

    if (!m_aircraftInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() <<"Failed to open flight data file " << m_aircraftInfoFile.fileName() << m_aircraftInfoFile.errorString();
        return;
    }

    qInfo() << "Opened aircraft info file " << m_aircraftInfoFile.fileName();
    m_aircraftInfoStream.setDevice(&m_aircraftInfoFile);


    // flight data columns
    m_flightDataStream << "timeMs";
    for (const FlightDataFile::Column_t& column : FlightDataFile::kColumns) {
        m_flightDataStream << "," << column.name;
    }
    m_flightDataStream << "\n";

    // Flight data
    qInfo() << "Recording flight data...";

    m_clock.start();
    m_flushTimer.start();
}


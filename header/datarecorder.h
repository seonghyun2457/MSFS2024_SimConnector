#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QString>

#include "flightData.h"
#include "aircraftInfo.h"

#include <iterator>

namespace FlightDataFile
{
constexpr const char* kTimeColumn = "timeMs";

typedef struct Column {
    const char* name;
    float FlightData_t::* member;
} Column_t;

// Order here = column order in the file. Names = header row.
constexpr Column_t kColumns[] = {
    {"pressureAltitudeFt",  &FlightData_t::pressureAltitudeFt},
    {"baroAltitudeFt",      &FlightData_t::baroAltitudeFt},
    {"mach",                &FlightData_t::mach},
    {"computedAirspeedKt",  &FlightData_t::computedAirspeedKt},
    {"trueAirspeedKt",      &FlightData_t::trueAirspeedKt},
    {"totalAirTempC",       &FlightData_t::totalAirTempC},
    {"verticalSpeedFpm",    &FlightData_t::verticalSpeedFpm},
    {"staticAirTempC",      &FlightData_t::staticAirTempC},
    {"angleOfAttackDeg",    &FlightData_t::angleOfAttackDeg},
    {"radioHeightFt",       &FlightData_t::radioHeightFt},
    {"latitudeDeg",         &FlightData_t::latitudeDeg},
    {"longitudeDeg",        &FlightData_t::longitudeDeg},
    {"groundSpeedKt",       &FlightData_t::groundSpeedKt},
    {"trackTrueDeg",        &FlightData_t::trackTrueDeg},
    {"headingTrueDeg",      &FlightData_t::headingTrueDeg},
    {"headingMagDeg",       &FlightData_t::headingMagDeg},
    {"pitchDeg",            &FlightData_t::pitchDeg},
    {"rollDeg",             &FlightData_t::rollDeg},
    {"selectedHeadingDeg",  &FlightData_t::selectedHeadingDeg},
    {"selectedAltitudeFt",  &FlightData_t::selectedAltitudeFt},
    {"localizerDeviation",  &FlightData_t::localizerDeviation},
    {"glideslopeDeviation", &FlightData_t::glideslopeDeviation},
    };


static_assert(std::size(kColumns) * sizeof(float) == sizeof(FlightData_t), "FlightData_t and FlightDataFile::kColumns are out of sync");
}


class dataRecorder final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(dataRecorder)

public:
    explicit dataRecorder(QObject *parent = nullptr);
    virtual ~dataRecorder() override;

signals:

public slots:
    void onFlightData(const FlightData_t& iFlightData);
    void onAircraftInfo(const AircraftInfo_t& iAircraftInfo);
    void record();
    void stopRecording();

private:
    static constexpr int FLUSH_INTERVAL{1000};

private:
    QFile m_flightDataFile;
    QTextStream m_flightDataStream;

    QFile m_aircraftInfoFile;
    QTextStream m_aircraftInfoStream;

    QElapsedTimer m_clock;
    QElapsedTimer m_flushTimer;
};

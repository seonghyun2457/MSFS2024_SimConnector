#include "simconnector.h"

#include <QDebug>

#include <iterator>

namespace
{
// Raw layout received from MSFS. Must match kSimVars below, entry for entry.
#pragma pack(push, 1)
struct RawFlightData {
    double pressureAltitude;
    double indicatedAltitude;
    double mach;
    double ias;
    double tas;
    double tat;
    double verticalSpeed;
    double sat;
    double radioHeight;
    double latitude;
    double longitude;
    double groundSpeed;
    double trackTrue;
    double headingTrue;
    double headingMag;
    double pitch;          // MSFS: + = nose DOWN
    double bank;           // MSFS: + = LEFT wing down
    double apHeading;
    double apAltitude;
    double navCdi;         // -127 .. +127
    double navGsi;         // -119 .. +119
    double angleOfAttack;
};
#pragma pack(pop)

typedef struct SimVar {
    const char* name{nullptr};
    const char* unit{nullptr};
} SimVar_t;

constexpr SimVar kFlightDataVars[] = {
    {"PRESSURE ALTITUDE",              "feet"},
    {"INDICATED ALTITUDE",             "feet"},
    {"AIRSPEED MACH",                  "mach"},
    {"AIRSPEED INDICATED",             "knots"},
    {"AIRSPEED TRUE",                  "knots"},
    {"TOTAL AIR TEMPERATURE",          "celsius"},
    {"VERTICAL SPEED",                 "feet per minute"},
    {"AMBIENT TEMPERATURE",            "celsius"},
    {"RADIO HEIGHT",                   "feet"},
    {"PLANE LATITUDE",                 "degrees"},
    {"PLANE LONGITUDE",                "degrees"},
    {"GROUND VELOCITY",                "knots"},
    {"GPS GROUND TRUE TRACK",          "degrees"},
    {"PLANE HEADING DEGREES TRUE",     "degrees"},
    {"PLANE HEADING DEGREES MAGNETIC", "degrees"},
    {"PLANE PITCH DEGREES",            "degrees"},
    {"PLANE BANK DEGREES",             "degrees"},
    {"AUTOPILOT HEADING LOCK DIR",     "degrees"},
    {"AUTOPILOT ALTITUDE LOCK VAR",    "feet"},
    {"NAV CDI:1",                      "number"},
    {"NAV GSI:1",                      "number"},
    {"INCIDENCE ALPHA",                "degrees"}
};

static_assert(sizeof(RawFlightData) == std::size(kFlightDataVars) * sizeof(double), "RawFlightData and kSimVars are out of sync");

#pragma pack(push, 1)
struct RawAircraftInfo {
    double designSpeedVc;
    double designSpeedVs1;
    double designSpeedVs0;
    double engMaxRpm;
};
#pragma pack(pop)

constexpr SimVar kAircraftInfoVars[] = {
    {"DESIGN SPEED VC",  "knots"},
    {"DESIGN SPEED VS1", "knots"},
    {"DESIGN SPEED VS0", "knots"},
    {"ENG MAX RPM:1",    "rpm"},
};

static_assert(sizeof(RawAircraftInfo) == std::size(kAircraftInfoVars) * sizeof(double), "RawAircraftInfo and kAircraftInfoVars are out of sync");
} // namespace

SimConnector::SimConnector(QObject* iParent)
    : QObject(iParent)
{
    // Auto-reset event
    m_hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_hEvent) qWarning() << "Create Event failed: " << GetLastError();
    m_eventNotifier.setHandle(m_hEvent);
    m_eventNotifier.setEnabled(false); // set to false by default
    connect(&m_eventNotifier, &QWinEventNotifier::activated, this, &SimConnector::pollDispatch);

    // Timer to connect MSFS
    m_connectorTimer.setInterval(CONNECTING_TIME_INTERVAL);
    connect(&m_connectorTimer, &QTimer::timeout, this, &SimConnector::tryConnect);

    // Watchdog timer: detect MSFS crashing / being killed
    m_watchdogTimer.setInterval(WATCHDOG_INTERVAL);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &SimConnector::checkAlive);
}

SimConnector::~SimConnector()
{
    m_connectorTimer.stop();
    closeHandle();

    if (m_hEvent) {
        CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }

    qInfo() << "Destruct SimConnector";
}

void SimConnector::execute()
{
    tryConnect();

    if (!isConnected()) m_connectorTimer.start();
}

void SimConnector::stop()
{
    m_connectorTimer.stop();

    if (isConnected()) {
        closeHandle();
        emit disconnected();
    }
}

void SimConnector::tryConnect()
{
    if (isConnected()) return;

    HRESULT hr = SimConnect_Open(&m_hSimConnect, "MSFS2024_SimConnector", nullptr, 0, m_hEvent, 0);

    if (FAILED(hr)) {
        m_hSimConnect = nullptr;
        qInfo() << "MSFS not available. retrying...";
        return;
    }

    qInfo() << "SimConnect handle opened";

    // Stop connector timer
    m_connectorTimer.stop();

    // Activate watchdog
    m_lastMessage.start();
    m_watchdogTimer.start();

    // Activate event
    m_eventNotifier.setEnabled(true);

    pollDispatch();
}

void SimConnector::pollDispatch()
{
    SIMCONNECT_RECV* data = nullptr;
    DWORD dataSize = 0;

    while (isConnected() && SUCCEEDED(SimConnect_GetNextDispatch(m_hSimConnect, &data, &dataSize))) {
        m_lastMessage.restart();
        handleMessage(data, dataSize);
    }
}

void SimConnector::checkAlive()
{
    if (!isConnected()) return;

    const int timeout = m_isOpen ? WATCHDOG_TIMEOUT : HANDSHAKE_TIMEOUT;

    if (m_lastMessage.elapsed() > timeout) {
        handleConnectionLost("MSFS not responding!");
        return;
    }

    if (FAILED(SimConnect_RequestSystemState(m_hSimConnect, eRequestId::REQ_SYSTEM_STATE, "Sim"))) {
        handleConnectionLost("Connection to MSFS broken");
    }
}

void SimConnector::handleMessage(SIMCONNECT_RECV* iData, const DWORD iDataSize)
{
    switch (iData->dwID) {
    case SIMCONNECT_RECV_ID_OPEN: {
        m_isOpen = true;
        auto* open = static_cast<SIMCONNECT_RECV_OPEN*>(iData);
        const QString name = QString::fromLatin1(open->szApplicationName);
        qDebug().noquote() << "Connected to" << name << QString("v%1.%2").arg(open->dwApplicationVersionMajor).arg(open->dwApplicationVersionMinor);
        registerDataDefinitions(); // re-register on every reconnect
        emit connected(name);
        break;
    }
    case SIMCONNECT_RECV_ID_EVENT_FILENAME: {
        SIMCONNECT_RECV_EVENT_FILENAME* event = static_cast<SIMCONNECT_RECV_EVENT_FILENAME*>(iData);
        if (event->uEventID == eEventId::EVT_AIRCRAFT_LOADED) {
            qInfo() << "Aircraft loaded: " << event->szFileName;
            requestAircraftInfo();
        }
        break;
    }
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
        handleSimObjectData(static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(iData), iDataSize);
        break;
    case SIMCONNECT_RECV_ID_SYSTEM_STATE:
        // Watchdog reply
        break;
    case SIMCONNECT_RECV_ID_QUIT:
        handleConnectionLost("MSFS closed");
        break;
    case SIMCONNECT_RECV_ID_EXCEPTION: {
        SIMCONNECT_RECV_EXCEPTION* exception = static_cast<SIMCONNECT_RECV_EXCEPTION*>(iData);
        qWarning() << "SimConnect exception" << exception->dwException << "sendID" << exception->dwSendID << "index" << exception->dwIndex;
        break;
    }
    default:
        break;
    }
}

void SimConnector::registerDataDefinitions()
{
    // FLIGHT DATA
    {
        // Register flight data
        for (const SimVar_t& flightDataVar : kFlightDataVars) {
            SimConnect_AddToDataDefinition(m_hSimConnect, eDefinitionId::DEF_FLIGHT_DATA, flightDataVar.name, flightDataVar.unit, SIMCONNECT_DATATYPE_FLOAT64);
        }

        // Send the user aircraft's data every simulation frame
        SimConnect_RequestDataOnSimObject(m_hSimConnect, eRequestId::REQ_FLIGHT_DATA, eDefinitionId::DEF_FLIGHT_DATA, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);
    }

    // AIRCRAFT INFO
    {
        // Register aircraft info
        for (const SimVar& aircraftInfoVar : kAircraftInfoVars) {
            SimConnect_AddToDataDefinition(m_hSimConnect, eDefinitionId::DEF_AIRCRAFT_INFO, aircraftInfoVar.name, aircraftInfoVar.unit, SIMCONNECT_DATATYPE_FLOAT64);
        }

        // Subscribe aircraft info
        SimConnect_SubscribeToSystemEvent(m_hSimConnect, eEventId::EVT_AIRCRAFT_LOADED, "AircraftLoaded");

        // Request aircraft info (get notify when aircraft get changed)
        requestAircraftInfo();
    }
}

void SimConnector::requestAircraftInfo()
{
    SimConnect_RequestDataOnSimObject(m_hSimConnect, eRequestId::REQ_AIRCRAFT_INFO, eDefinitionId::DEF_AIRCRAFT_INFO, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE);
}

void SimConnector::handleSimObjectData(const SIMCONNECT_RECV_SIMOBJECT_DATA* iData, const DWORD iDataSize)
{
    const BYTE* begin   = reinterpret_cast<const BYTE*>(iData);
    const BYTE* payload = reinterpret_cast<const BYTE*>(&iData->dwData);
    const DWORD headerSize = static_cast<DWORD>(payload - begin);
    const DWORD payloadSize = iDataSize - headerSize;

    switch (iData->dwRequestID) {
    case eRequestId::REQ_FLIGHT_DATA:
        if (payloadSize >= sizeof(RawFlightData)) {
            handleFlightData(payload);
        } else {
            qWarning() << "Flight data too small: " << iDataSize;
        }
        break;
    case eRequestId::REQ_AIRCRAFT_INFO:
        if (payloadSize >= sizeof(RawAircraftInfo)) {
            handleAircraftInfo(payload);
        } else {
            qWarning() << "Aircraft Info too small: " << iDataSize;
        }
        break;
    default:
        break;
    }
}

void SimConnector::handleFlightData(const BYTE* iPayload)
{
    const RawFlightData* pRawFlightData = reinterpret_cast<const RawFlightData*>(iPayload);

    FlightData_t out;

    out.pressureAltitudeFt  = pRawFlightData->pressureAltitude;
    out.baroAltitudeFt      = pRawFlightData->indicatedAltitude;
    out.mach                = pRawFlightData->mach;
    out.computedAirspeedKt  = pRawFlightData->ias;
    out.trueAirspeedKt      = pRawFlightData->tas;
    out.totalAirTempC       = pRawFlightData->tat;
    out.verticalSpeedFpm    = pRawFlightData->verticalSpeed;
    out.staticAirTempC      = pRawFlightData->sat;
    out.angleOfAttackDeg    = pRawFlightData->angleOfAttack;
    out.radioHeightFt       = pRawFlightData->radioHeight;
    out.latitudeDeg         = pRawFlightData->latitude;
    out.longitudeDeg        = pRawFlightData->longitude;
    out.groundSpeedKt       = pRawFlightData->groundSpeed;
    out.trackTrueDeg        = pRawFlightData->trackTrue;
    out.headingTrueDeg      = pRawFlightData->headingTrue;
    out.headingMagDeg       = pRawFlightData->headingMag;
    out.pitchDeg            = -pRawFlightData->pitch;   // to nose-up +
    out.rollDeg             = -pRawFlightData->bank;    // to right-wing-down +
    out.selectedHeadingDeg  = pRawFlightData->apHeading;
    out.selectedAltitudeFt  = pRawFlightData->apAltitude;
    out.localizerDeviation  = pRawFlightData->navCdi / 127.0;
    out.glideslopeDeviation = pRawFlightData->navGsi / 119.0;

    emit flightDataReceived(out);
}

void SimConnector::handleAircraftInfo(const BYTE* iPayload)
{
    const RawAircraftInfo* pAircraftInfo = reinterpret_cast<const RawAircraftInfo*>(iPayload);

    AircraftInfo_t out;

    out.cruiseSpeedKt     = pAircraftInfo->designSpeedVc;
    out.stallSpeedCleanKt = pAircraftInfo->designSpeedVs1;
    out.stallSpeedFlapsKt = pAircraftInfo->designSpeedVs0;
    out.engineMaxRpm      = pAircraftInfo->engMaxRpm;

    emit aircraftInfoReceived(out);
}

void SimConnector::handleConnectionLost(const char *iReason)
{
    qWarning() << iReason;
    closeHandle();
    emit disconnected();

    // restart connector timer
    m_connectorTimer.start();
}

void SimConnector::closeHandle()
{
    m_isOpen = false;

    m_watchdogTimer.stop();
    m_eventNotifier.setEnabled(false);

    if (m_hSimConnect) {
        SimConnect_Close(m_hSimConnect);
        m_hSimConnect = nullptr;
        qInfo() << "Handle closed";
    }
}
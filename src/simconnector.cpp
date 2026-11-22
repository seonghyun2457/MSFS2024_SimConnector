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

constexpr SimVar kSimVars[] = {
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

static_assert(sizeof(RawFlightData) == std::size(kSimVars) * sizeof(double), "RawFlightData and kSimVars are out of sync");
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
        auto* ex = static_cast<SIMCONNECT_RECV_EXCEPTION*>(iData);
        qWarning() << "SimConnect exception" << ex->dwException << "sendID" << ex->dwSendID << "index" << ex->dwIndex;
        break;
    }
    default:
        break;
    }
}

void SimConnector::registerDataDefinitions()
{
    for (const SimVar_t& simVar : kSimVars) {
        SimConnect_AddToDataDefinition(m_hSimConnect, eDefinitionId::DEF_FLIGHT_DATA, simVar.name, simVar.unit, SIMCONNECT_DATATYPE_FLOAT64);
    }

    // Send the user aircraft's data every simulation frame
    SimConnect_RequestDataOnSimObject(m_hSimConnect, eRequestId::REQ_FLIGHT_DATA, eDefinitionId::DEF_FLIGHT_DATA, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);
}

void SimConnector::handleSimObjectData(const SIMCONNECT_RECV_SIMOBJECT_DATA* iData, const DWORD iDataSize)
{
    if (iData->dwRequestID != eRequestId::REQ_FLIGHT_DATA) return;

    const BYTE* begin   = reinterpret_cast<const BYTE*>(iData);
    const BYTE* payload = reinterpret_cast<const BYTE*>(&iData->dwData);
    const DWORD headerSize = static_cast<DWORD>(payload - begin);

    if (iDataSize < headerSize + sizeof(RawFlightData)) {
        qWarning() << "SimObject data too small: " << iDataSize;
        return;
    }

    const RawFlightData* pRawFlightData = reinterpret_cast<const RawFlightData*>(&iData->dwData);


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
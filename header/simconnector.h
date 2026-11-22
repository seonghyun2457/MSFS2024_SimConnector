#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <SimConnect.h>

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>
#include <QWinEventNotifier>

#include "flightData.h"

enum eDefinitionId : SIMCONNECT_DATA_DEFINITION_ID {
    DEF_FLIGHT_DATA = 1
};

enum eRequestId    : SIMCONNECT_DATA_REQUEST_ID {
    REQ_FLIGHT_DATA = 1,
    REQ_SYSTEM_STATE = 2
};

class SimConnector final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SimConnector)

public:
    explicit SimConnector(QObject* iParent = nullptr);
    virtual ~SimConnector() override;


    void execute();
    void stop();
    bool isConnected() const { return m_hSimConnect != nullptr; }

signals:
    void connected(const QString& iSimName);
    void disconnected();
    void flightDataReceived(const FlightData_t& iFlightData);

private slots:
    void tryConnect();
    void pollDispatch();
    void checkAlive();


private:
    void handleMessage(SIMCONNECT_RECV* iData, const DWORD iDataSize);
    void handleSimObjectData(const SIMCONNECT_RECV_SIMOBJECT_DATA* iData, const DWORD iDataSize);
    void registerDataDefinitions();
    void handleConnectionLost(const char* iReason);
    void closeHandle();

private:
    static constexpr int CONNECTING_TIME_INTERVAL{5000}; // Retry connection every 5 seconds
    static constexpr int WATCHDOG_INTERVAL{2000};        // ping MSFS every 2 seconds
    static constexpr int WATCHDOG_TIMEOUT{5000};         // MSFS connection lost after 5 seconds
    static constexpr int HANDSHAKE_TIMEOUT{60000};       // Maxium opening time

private:
    bool m_isOpen{false};
    HANDLE m_hSimConnect{nullptr};
    HANDLE m_hEvent{nullptr};
    QWinEventNotifier m_eventNotifier;
    QTimer m_connectorTimer;
    QTimer m_watchdogTimer;
    QElapsedTimer m_lastMessage;
};

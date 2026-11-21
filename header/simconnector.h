#ifndef SIMCONNECTOR_H
#define SIMCONNECTOR_H

class SimConnector final
{
public:
    SimConnector() = default;
    virtual ~SimConnector() = default;

    SimConnector(const SimConnector& iOther) = delete;
    SimConnector& operator=(const SimConnector& iOther) = delete;

    SimConnector(SimConnector&& iOther) = delete;
    SimConnector& operator=(SimConnector&& iOther) = delete;
};

#endif // SIMCONNECTOR_H

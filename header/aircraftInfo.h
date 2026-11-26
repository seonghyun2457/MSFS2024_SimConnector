#pragma once

// Aircraft constants (refreshed when the aircraft changes)
typedef struct AircraftInfo {
    float cruiseSpeedKt      = 0.f;   // DESIGN SPEED VC
    float stallSpeedCleanKt  = 0.f;   // DESIGN SPEED VS1
    float stallSpeedFlapsKt  = 0.f;   // DESIGN SPEED VS0
    float engineMaxRpm       = 0.f;   // ENG MAX RPM:1
} AircraftInfo_t;
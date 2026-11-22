#pragma once

// Aircraft state in aviation conventions (nose-up +, right-wing-down +).
// The comment on each field is the ARINC 429 label (octal) it will be encoded into.
typedef struct FlightData {
    // Air data
    double pressureAltitudeFt   = 0.0;  // 203
    double baroAltitudeFt       = 0.0;  // 204
    double mach                 = 0.0;  // 205
    double computedAirspeedKt   = 0.0;  // 206
    double trueAirspeedKt       = 0.0;  // 210
    double totalAirTempC        = 0.0;  // 211
    double verticalSpeedFpm     = 0.0;  // 212
    double staticAirTempC       = 0.0;  // 213
    double angleOfAttackDeg     = 0.0;  // 221

    // Radio altimeter
    double radioHeightFt        = 0.0;  // 164

    // Inertial / position
    double latitudeDeg          = 0.0;  // 310
    double longitudeDeg         = 0.0;  // 311
    double groundSpeedKt        = 0.0;  // 312
    double trackTrueDeg         = 0.0;  // 313
    double headingTrueDeg       = 0.0;  // 314
    double headingMagDeg        = 0.0;  // 320
    double pitchDeg             = 0.0;  // 324  (+ = nose up)
    double rollDeg              = 0.0;  // 325  (+ = right wing down)

    // Autopilot selections
    double selectedHeadingDeg   = 0.0;  // 101
    double selectedAltitudeFt   = 0.0;  // 102

    // ILS deviation, normalized -1.0 .. +1.0 (full-scale deflection)
    double localizerDeviation   = 0.0;  // 173
    double glideslopeDeviation  = 0.0;  // 174
} FlightData_t;

// Aircraft constants (refreshed when the aircraft changes)
typedef struct AircraftInfo {
    double cruiseSpeedKt      = 0.0;   // DESIGN SPEED VC
    double stallSpeedCleanKt  = 0.0;   // DESIGN SPEED VS1
    double stallSpeedFlapsKt  = 0.0;   // DESIGN SPEED VS0
    double engineMaxRpm       = 0.0;   // ENG MAX RPM:1
} AircraftInfo_t;
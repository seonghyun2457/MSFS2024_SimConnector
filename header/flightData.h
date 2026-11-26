#pragma once

// Aircraft state in aviation conventions (nose-up +, right-wing-down +).
// The comment on each field is the ARINC 429 label (octal) it will be encoded into.
typedef struct FlightData {
    // Air data
    float pressureAltitudeFt   = 0.f;  // 203
    float baroAltitudeFt       = 0.f;  // 204
    float mach                 = 0.f;  // 205
    float computedAirspeedKt   = 0.f;  // 206
    float trueAirspeedKt       = 0.f;  // 210
    float totalAirTempC        = 0.f;  // 211
    float verticalSpeedFpm     = 0.f;  // 212
    float staticAirTempC       = 0.f;  // 213
    float angleOfAttackDeg     = 0.f;  // 221

    // Radio altimeter
    float radioHeightFt        = 0.f;  // 164

    // Inertial / position
    float latitudeDeg          = 0.f;  // 310
    float longitudeDeg         = 0.f;  // 311
    float groundSpeedKt        = 0.f;  // 312
    float trackTrueDeg         = 0.f;  // 313
    float headingTrueDeg       = 0.f;  // 314
    float headingMagDeg        = 0.f;  // 320
    float pitchDeg             = 0.f;  // 324  (+ = nose up)
    float rollDeg              = 0.f;  // 325  (+ = right wing down)

    // Autopilot selections
    float selectedHeadingDeg   = 0.f;  // 101
    float selectedAltitudeFt   = 0.f;  // 102

    // ILS deviation, normalized -1.0 .. +1.0 (full-scale deflection)
    float localizerDeviation   = 0.f;  // 173
    float glideslopeDeviation  = 0.f;  // 174
} FlightData_t;
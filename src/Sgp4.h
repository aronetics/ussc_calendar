#pragma once
#include <string>
#include <cmath>

// ── TLE ──────────────────────────────────────────────────────────────────────

struct TleData {
    std::string name;
    std::string line1;
    std::string line2;
    int    noradId    = 0;
    double epochJd    = 0.0;   // Julian date of TLE epoch
    double bstar      = 0.0;   // drag term (1/Earth-radii)
    double inclinDeg  = 0.0;
    double raanDeg    = 0.0;
    double ecc        = 0.0;
    double argPgDeg   = 0.0;
    double meanAnomDeg= 0.0;
    double meanMotion = 0.0;   // rev/day
    bool   valid      = false;
};

// ── Coordinate types ─────────────────────────────────────────────────────────

struct EciState {
    double x  = 0, y  = 0, z  = 0;    // km, ECI
    double vx = 0, vy = 0, vz = 0;    // km/s
};

struct GeoPos {
    double latDeg = 0;    // geodetic, N positive
    double lonDeg = 0;    // E positive
    double altKm  = 0;    // above WGS84 ellipsoid
};

struct LookAngles {
    double azDeg  = 0;    // 0=N, 90=E
    double elDeg  = 0;    // above horizon
    double rangeKm= 0;
};

// ── API ───────────────────────────────────────────────────────────────────────

// Parse three-line TLE (name, line1, line2)
TleData ParseTle(const std::string& name,
                 const std::string& line1,
                 const std::string& line2);

// Julian date from calendar components (UTC)
double JdFromCalendar(int y, int mon, int d, int h, int min, double sec);

// GMST in radians for a Julian date
double GmstRad(double jd);

// SGP4 propagate: tsince = minutes since TLE epoch
// Returns ECI state in km / km·s⁻¹; all-zero on failure (degenerate orbit)
EciState Sgp4Propagate(const TleData& tle, double tsince);

// Convert ECI + JD → geodetic
GeoPos EciToGeo(const EciState& eci, double jd);

// Look angles from observer to satellite (both in geodetic)
LookAngles LookAnglesTo(const GeoPos& obs, const GeoPos& sat);

// Combined: ECI state → look angles from observer (faster, avoids double conversion)
LookAngles LookAnglesEci(const GeoPos& obs, const EciState& satEci, double jd);

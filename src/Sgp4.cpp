#include "Sgp4.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <algorithm>

// ── Constants (NORAD/WGS84) ──────────────────────────────────────────────────

static const double kPI      = 3.141592653589793;
static const double kTWOPI   = 2.0 * kPI;
static const double kDE2RA   = kPI / 180.0;
static const double kXKE     = 0.0743669161;      // sqrt(GM) in ER^1.5 / min
static const double kXKMPER  = 6378.137;           // Earth equatorial radius, km (WGS84)
static const double kAE      = 1.0;                // distance in Earth radii
static const double kJ2      = 1.082616e-3;
static const double kJ3      = -2.53881e-6;
static const double kJ4      = -1.65597e-6;
static const double kCK2     = 0.5 * kJ2 * kAE * kAE;
static const double kCK4     = -0.375 * kJ4 * kAE * kAE * kAE * kAE;
static const double kS       = kAE + 78.0 / kXKMPER;   // (1 + 78/Re), s parameter
static const double kQOMS2T  = std::pow((120.0 - 78.0) / kXKMPER, 4.0);  // (q0-s)^4
static const double kA3OVK2  = -kJ3 / kCK2 * kAE * kAE * kAE;
static const double kMINPDAY = 1440.0;             // minutes per day
static const double kF       = 1.0 / 298.257223563; // WGS84 flattening
static const double kF2      = kF * (2.0 - kF);    // e² of Earth ellipsoid

// ── TLE field parsers ─────────────────────────────────────────────────────────

static double FieldDouble(const std::string& line, int col, int len)
{
    if ((int)line.size() < col + len) return 0.0;
    const char* p = line.c_str() + col;
    char buf[24] = {};
    int n = std::min(len, 23);
    for (int i = 0; i < n; ++i) buf[i] = p[i];
    return std::atof(buf);
}

// "Decimal-point-implied" + exponent field: "±DDDDD±E" → ±0.DDDDD×10^(±E)
static double FieldImplied(const std::string& line, int col, int len)
{
    if ((int)line.size() < col + len) return 0.0;
    std::string s = line.substr(col, len);

    bool neg = false;
    int i = 0;
    while (i < (int)s.size() && s[i] == ' ') ++i;
    if (i < (int)s.size() && s[i] == '-') { neg = true; ++i; }
    else if (i < (int)s.size() && s[i] == '+') { ++i; }

    std::string mant;
    while (i < (int)s.size() && std::isdigit((unsigned char)s[i])) mant += s[i++];

    int esign = 1;
    if (i < (int)s.size() && s[i] == '-') { esign = -1; ++i; }
    else if (i < (int)s.size() && s[i] == '+') { ++i; }

    int exp = 0;
    while (i < (int)s.size() && std::isdigit((unsigned char)s[i]))
        exp = exp * 10 + (s[i++] - '0');

    if (mant.empty()) return 0.0;
    double val = std::atof(("0." + mant).c_str());
    if (neg) val = -val;
    return val * std::pow(10.0, esign * exp);
}

// ── Julian date ───────────────────────────────────────────────────────────────

double JdFromCalendar(int y, int mon, int d, int h, int min, double sec)
{
    if (mon <= 2) { --y; mon += 12; }
    int A = y / 100;
    int B = 2 - A + A / 4;
    return std::floor(365.25 * (y + 4716))
         + std::floor(30.6001 * (mon + 1))
         + d + B - 1524.5
         + (h + min / 60.0 + sec / 3600.0) / 24.0;
}

double GmstRad(double jd)
{
    double T = (jd - 2451545.0) / 36525.0;
    double gmst = 280.46061837
                + 360.98564736629 * (jd - 2451545.0)
                + T * T * 0.000387933
                - T * T * T / 38710000.0;
    gmst = std::fmod(gmst, 360.0);
    if (gmst < 0.0) gmst += 360.0;
    return gmst * kDE2RA;
}

// ── TLE parsing ───────────────────────────────────────────────────────────────

TleData ParseTle(const std::string& name,
                 const std::string& line1,
                 const std::string& line2)
{
    TleData t;
    t.name  = name;
    t.line1 = line1;
    t.line2 = line2;

    if ((int)line1.size() < 69 || (int)line2.size() < 69) return t;
    if (line1[0] != '1' || line2[0] != '2') return t;

    t.noradId = (int)FieldDouble(line1, 2, 5);

    // Epoch: YYDDD.FFFFFFFF at col 18, length 14
    double epochRaw = FieldDouble(line1, 18, 14);
    int yr2 = (int)(epochRaw / 1000.0);
    double dayOfYear = epochRaw - yr2 * 1000.0;
    int fullYear = (yr2 >= 57) ? (1900 + yr2) : (2000 + yr2);

    // JD of Jan 1.0 of fullYear (day 1 = Jan 1)
    double jd1 = JdFromCalendar(fullYear, 1, 1, 0, 0, 0.0);
    t.epochJd = jd1 + (dayOfYear - 1.0);

    t.bstar       = FieldImplied(line1, 53, 8);
    t.inclinDeg   = FieldDouble(line2,  8, 8);
    t.raanDeg     = FieldDouble(line2, 17, 8);
    t.argPgDeg    = FieldDouble(line2, 34, 8);
    t.meanAnomDeg = FieldDouble(line2, 43, 8);
    t.meanMotion  = FieldDouble(line2, 52, 11); // rev/day

    // Eccentricity: 7 digits with implied decimal: "0001234" → 0.0001234
    if ((int)line2.size() >= 34) {
        std::string eStr;
        for (int c = 26; c < 33 && c < (int)line2.size(); ++c) {
            if (std::isdigit((unsigned char)line2[c])) eStr += line2[c];
        }
        if (!eStr.empty())
            t.ecc = std::atof(("0." + eStr).c_str());
    }

    t.valid = (t.meanMotion > 0.0 && t.ecc >= 0.0 && t.ecc < 1.0);
    return t;
}

// ── SGP4 pre-computed state ───────────────────────────────────────────────────

struct Sgp4Init {
    double n0pp, a0pp, e0, i0, omega0, raan0, M0, bstar;
    double xmdot, omgdot, xnodot, xnodcf;
    double C1, C2, C4, C5, D2, D3, D4;
    double t2cof, t3cof, t4cof, t5cof;
    double eta, omgcof, xmcof, xlcof, aycof;
    double delmo, sinmo;
    double x1mth2;
    bool   isimp;
};

static Sgp4Init MakeInit(const TleData& tle)
{
    Sgp4Init st = {};
    st.e0    = tle.ecc;
    st.i0    = tle.inclinDeg  * kDE2RA;
    st.omega0= tle.argPgDeg   * kDE2RA;
    st.raan0 = tle.raanDeg    * kDE2RA;
    st.M0    = tle.meanAnomDeg* kDE2RA;
    st.bstar = tle.bstar;

    double n0 = tle.meanMotion * kTWOPI / kMINPDAY;  // rad/min
    double e0 = st.e0;
    double cosio  = std::cos(st.i0);
    double sinio  = std::sin(st.i0);
    double theta2 = cosio * cosio;
    double x3thm1 = 3.0 * theta2 - 1.0;
    double betao2 = 1.0 - e0 * e0;
    double betao  = std::sqrt(betao2);

    // Recover original mean motion (Kozai → Brouwer)
    double a1   = std::pow(kXKE / n0, 2.0/3.0);
    double del1 = 1.5 * kCK2 * x3thm1 / (a1 * a1 * betao * betao2);
    double a0   = a1 * (1.0 - del1/3.0 - del1*del1 - 134.0/81.0*del1*del1*del1);
    double del0 = 1.5 * kCK2 * x3thm1 / (a0 * a0 * betao * betao2);
    st.n0pp = n0 / (1.0 + del0);
    st.a0pp = a0 / (1.0 - del0);

    double perigee_km = (st.a0pp * (1.0 - e0) - kAE) * kXKMPER;

    double s4, qoms24;
    if (perigee_km < 98.0) {
        s4     = 20.0 / kXKMPER + kAE;
        qoms24 = std::pow((120.0 - 20.0) / kXKMPER, 4.0);
    } else if (perigee_km < 156.0) {
        s4     = perigee_km / kXKMPER + kAE - 1.0 + 78.0/kXKMPER;
        // s = 1 + (perigee-78)/XKMPER
        s4     = 1.0 + (perigee_km - 78.0) / kXKMPER;
        qoms24 = std::pow((120.0 - perigee_km) / kXKMPER, 4.0);
    } else {
        s4     = kS;
        qoms24 = kQOMS2T;
    }

    double pinvsq = 1.0 / (st.a0pp * st.a0pp * betao2 * betao2);
    double tsi    = 1.0 / (st.a0pp - s4);
    double eta    = st.a0pp * e0 * tsi;
    double etasq  = eta * eta;
    double eeta   = e0 * eta;
    double psisq  = std::abs(1.0 - etasq);
    double coef   = qoms24 * std::pow(tsi, 4.0);
    double coef1  = coef / std::pow(psisq, 3.5);

    st.x1mth2 = 1.0 - theta2;

    st.C2 = coef1 * st.n0pp * (
        st.a0pp * (1.0 + 1.5*etasq + eeta*(4.0 + etasq)) +
        0.75 * kCK2 * tsi / psisq * x3thm1 *
            (8.0 + 3.0*etasq*(8.0 + etasq))
    );
    st.C1 = st.bstar * st.C2;

    double C3 = 0.0;
    if (e0 > 1.0e-4)
        C3 = coef * tsi * kA3OVK2 * st.n0pp * sinio / e0;

    st.C4 = 2.0 * st.n0pp * coef1 * st.a0pp * betao2 * (
        eta * (2.0 + 0.5*etasq) + e0 * (0.5 + 2.0*etasq) -
        2.0 * kCK2 * tsi / (st.a0pp * psisq) * (
            -3.0 * x3thm1 * (1.0 - 2.0*eeta + etasq*(1.5 - 0.5*eeta)) +
             0.75 * st.x1mth2 * (2.0*etasq - eeta*(1.0 + etasq)) *
                 std::cos(2.0 * st.omega0)
        )
    );
    st.C5 = 2.0 * coef1 * st.a0pp * betao2 *
            (1.0 + 2.75*(etasq + eeta) + eeta*etasq);

    st.isimp = (perigee_km < 220.0);

    if (!st.isimp) {
        double C12 = st.C1 * st.C1;
        st.D2 = 4.0 * st.a0pp * tsi * C12;
        st.D3 = (4.0/3.0) * st.a0pp * tsi * tsi * (17.0*st.a0pp + s4) * st.C1 * C12;
        st.D4 = (2.0/3.0) * st.a0pp * st.a0pp * tsi*tsi*tsi *
                (221.0*st.a0pp + 31.0*s4) * C12 * C12;
    }

    // Secular drift rates
    double temp1  = 3.0 * kCK2 * pinvsq * st.n0pp;
    double temp2  = temp1 * kCK2 * pinvsq;
    double temp3  = 1.25 * kCK4 * pinvsq * pinvsq * st.n0pp;

    st.xmdot  = st.n0pp
              + 0.5 * temp1 * betao * x3thm1
              + 0.0625 * temp2 * betao * (13.0 - 78.0*theta2 + 137.0*theta2*theta2);

    st.omgdot = -0.5 * temp1 * (1.0 - 5.0*theta2)
              + 0.0625 * temp2 * (7.0 - 114.0*theta2 + 395.0*theta2*theta2)
              + temp3 * (3.0 - 36.0*theta2 + 49.0*theta2*theta2);

    double xhdot1 = -temp1 * cosio;
    st.xnodot = xhdot1
              + (0.5*temp2*(4.0 - 19.0*theta2) +
                 2.0*temp3*(3.0 - 7.0*theta2)) * cosio;

    st.xnodcf = 3.5 * betao2 * xhdot1 * st.C1;
    st.t2cof  = 1.5 * st.C1;
    st.t3cof  = st.isimp ? 0.0 : st.D2 + 2.0*st.C1*st.C1;
    st.t4cof  = st.isimp ? 0.0 :
        0.25*(3.0*st.D3 + st.C1*(12.0*st.D2 + 10.0*st.C1*st.C1));
    st.t5cof  = st.isimp ? 0.0 :
        0.2*(3.0*st.D4 + 12.0*st.C1*st.D3 + 6.0*st.D2*st.D2 +
             15.0*st.C1*st.C1*(2.0*st.D2 + st.C1*st.C1));

    st.eta    = eta;
    st.omgcof = st.bstar * C3 * std::cos(st.omega0);
    st.xmcof  = (e0 > 1.0e-4) ?
        -2.0/3.0 * coef * st.bstar / eeta : 0.0;

    st.xlcof  = 0.125 * kA3OVK2 * sinio *
                (3.0 + 5.0*cosio) / (1.0 + cosio);
    st.aycof  = 0.25 * kA3OVK2 * sinio;
    st.delmo  = std::pow(1.0 + eta * std::cos(st.M0), 3.0);
    st.sinmo  = std::sin(st.M0);

    return st;
}

// ── SGP4 propagation ──────────────────────────────────────────────────────────

EciState Sgp4Propagate(const TleData& tle, double tsince)
{
    if (!tle.valid) return {};

    Sgp4Init s = MakeInit(tle);

    // Secular update
    double xmdf   = s.M0     + s.xmdot  * tsince;
    double omgadf = s.omega0 + s.omgdot * tsince;
    double xnoddf = s.raan0  + s.xnodot * tsince;
    double tsq    = tsince * tsince;

    double xnode = xnoddf + s.xnodcf * tsq;
    double omega, xmp;
    double tempa, tempe, templ;

    if (s.isimp) {
        omega = omgadf;
        xmp   = xmdf;
        tempa = 1.0 - s.C1 * tsince;
        tempe = s.bstar * s.C4 * tsince;
        templ = s.t2cof * tsq;
    } else {
        double delomg = s.omgcof * tsince;
        double delm   = s.xmcof *
            (std::pow(1.0 + s.eta * std::cos(xmdf), 3.0) - s.delmo);
        double temp   = delomg + delm;
        xmp   = xmdf   + temp;
        omega = omgadf - temp;
        double tcube = tsq * tsince;
        double tfour = tsince * tcube;
        tempa = 1.0 - s.C1*tsince - s.D2*tsq - s.D3*tcube - s.D4*tfour;
        tempe = s.bstar*(s.C4*tsince + s.C5*(std::sin(xmp) - s.sinmo));
        templ = s.t2cof*tsq + s.t3cof*tcube + tfour*(s.t4cof + tsince*s.t5cof);
    }

    double a = s.a0pp * tempa * tempa;
    double e = s.e0 - tempe;
    e = std::max(1.0e-6, std::min(e, 0.9999));
    double xl = xmp + omega + xnode + s.n0pp * templ;

    // Long-period periodics
    double axn  = e * std::cos(omega);
    double temp = 1.0 / (a * (1.0 - e*e));
    double ayn  = e * std::sin(omega) + s.aycof * temp;
    double xlt  = xl + s.xlcof * temp * axn;

    // Kepler: solve E'+ axn*sin(E') - ayn*cos(E') = capu
    double capu = std::fmod(xlt - xnode, kTWOPI);
    if (capu < 0.0) capu += kTWOPI;

    double epw = capu;
    double sinepw = 0.0, cosepw = 0.0;
    for (int ktr = 0; ktr < 10; ++ktr) {
        sinepw = std::sin(epw);
        cosepw = std::cos(epw);
        double f  = capu - epw + axn*sinepw - ayn*cosepw;
        double df = 1.0  - axn*cosepw - ayn*sinepw;
        if (std::abs(df) < 1.0e-12) break;
        double deps = f / df;
        if (std::abs(deps) < 1.0e-12) break;
        epw += deps;
    }

    // Short-period preliminary quantities
    double ecose = axn*cosepw + ayn*sinepw;
    double esine = axn*sinepw - ayn*cosepw;
    double el2   = axn*axn + ayn*ayn;
    double pl    = a * (1.0 - el2);
    if (pl < 0.0) return {};

    double r     = a * (1.0 - ecose);
    double rdot  = kXKE * std::sqrt(a) / r * esine;
    double rfdot = kXKE * std::sqrt(pl)  / r;

    // True longitude and U
    double betal  = std::sqrt(1.0 - el2);
    double cosu   = a/r * (cosepw - axn) + a*ayn*esine / (r*(1.0+betal));
    double sinu   = a/r * (sinepw - ayn) - a*axn*esine / (r*(1.0+betal));
    double u      = std::atan2(sinu, cosu);
    double sin2u  = 2.0 * sinu * cosu;
    double cos2u  = 2.0 * cosu * cosu - 1.0;

    // Short-period corrections (J2)
    double temp0  = kCK2 / (pl * pl);
    double rk = r*(1.0 - 1.5*temp0*betal*s.x1mth2*cos2u)
              + 0.5*kCK2/pl * s.x1mth2 * cos2u;
    double uk = u - 0.25*temp0*s.x1mth2*sin2u;   // corrected arg lat
    double xnodek = xnode + 1.5*temp0*std::cos(s.i0)*sin2u;
    double xinck  = s.i0  + 1.5*temp0*std::cos(s.i0)*std::sin(s.i0)*cos2u;
    double rdotk  = rdot  - s.n0pp*temp0*s.x1mth2*sin2u;
    double rfdotk = rfdot + s.n0pp*temp0*(s.x1mth2*cos2u + 1.5*s.x1mth2*s.x1mth2*0.0);

    // Unit vectors in ECI
    double sinuk = std::sin(uk), cosuk = std::cos(uk);
    double sinik = std::sin(xinck), cosik = std::cos(xinck);
    double sinxnk= std::sin(xnodek), cosxnk= std::cos(xnodek);

    double Mx = -sinxnk*cosik;  // M = nodal unit vector
    double My =  cosxnk*cosik;
    double Mz =  sinik;
    double Nx =  cosxnk;        // N = nodal unit vector
    double Ny =  sinxnk;
    double Nz =  0.0;

    // Unit vectors in orbital plane: U = along r, V = along velocity
    double Ux = Mx*sinuk + Nx*cosuk;
    double Uy = My*sinuk + Ny*cosuk;
    double Uz = Mz*sinuk + Nz*cosuk;
    double Vx = Mx*cosuk - Nx*sinuk;
    double Vy = My*cosuk - Ny*sinuk;
    double Vz = Mz*cosuk - Nz*sinuk;

    EciState eci;
    double rkm = rk * kXKMPER;   // convert Earth radii → km
    eci.x  = rkm * Ux;
    eci.y  = rkm * Uy;
    eci.z  = rkm * Uz;
    eci.vx = (rdotk*Ux + rfdotk*Vx) * kXKMPER / 60.0;  // ER/min → km/s
    eci.vy = (rdotk*Uy + rfdotk*Vy) * kXKMPER / 60.0;
    eci.vz = (rdotk*Uz + rfdotk*Vz) * kXKMPER / 60.0;
    return eci;
}

// ── Coordinate transforms ─────────────────────────────────────────────────────

GeoPos EciToGeo(const EciState& eci, double jd)
{
    double gmst = GmstRad(jd);

    // ECI → ECEF
    double cg = std::cos(gmst), sg = std::sin(gmst);
    double xe = eci.x*cg + eci.y*sg;
    double ye =-eci.x*sg + eci.y*cg;
    double ze = eci.z;

    double lon = std::atan2(ye, xe);
    double r   = std::sqrt(xe*xe + ye*ye);

    // Iterative geodetic lat (Bowring, 1 iteration sufficient)
    double lat = std::atan2(ze, r * (1.0 - kF2));
    for (int i = 0; i < 4; ++i) {
        double sl = std::sin(lat);
        double N  = kXKMPER / std::sqrt(1.0 - kF2 * sl*sl);
        lat = std::atan2(ze + kF2 * N * sl, r);
    }

    double sl = std::sin(lat);
    double N  = kXKMPER / std::sqrt(1.0 - kF2 * sl*sl);
    double cl = std::cos(lat);
    double alt = (cl > 0.01) ? r / cl - N
                              : ze / sl - N*(1.0 - kF2);

    GeoPos g;
    g.latDeg = lat / kDE2RA;
    g.lonDeg = lon / kDE2RA;
    g.altKm  = alt;
    return g;
}

LookAngles LookAnglesTo(const GeoPos& obs, const GeoPos& sat)
{
    // Simple spherical: convert both to ECEF, compute range + SEZ frame
    double latO = obs.latDeg * kDE2RA;
    double lonO = obs.lonDeg * kDE2RA;
    double latS = sat.latDeg * kDE2RA;
    double lonS = sat.lonDeg * kDE2RA;

    auto toEcef = [&](double lat, double lon, double alt,
                      double& x, double& y, double& z) {
        double sl = std::sin(lat), cl = std::cos(lat);
        double N  = kXKMPER / std::sqrt(1.0 - kF2 * sl*sl);
        x = (N + alt) * cl * std::cos(lon);
        y = (N + alt) * cl * std::sin(lon);
        z = (N*(1.0-kF2) + alt) * sl;
    };

    double ox, oy, oz, sx, sy, sz;
    toEcef(latO, lonO, obs.altKm, ox, oy, oz);
    toEcef(latS, lonS, sat.altKm, sx, sy, sz);

    double dx = sx-ox, dy = sy-oy, dz = sz-oz;

    double slO = std::sin(latO), clO = std::cos(latO);
    double slnO= std::sin(lonO), clnO= std::cos(lonO);

    // Transform to South-East-Zenith
    double S =  slO*clnO*dx + slO*slnO*dy - clO*dz;
    double E = -slnO*dx     + clnO*dy;
    double Z =  clO*clnO*dx + clO*slnO*dy + slO*dz;

    double rng = std::sqrt(S*S + E*E + Z*Z);
    LookAngles la;
    la.rangeKm = rng;
    la.elDeg   = std::asin(Z / rng) / kDE2RA;
    la.azDeg   = std::fmod(std::atan2(E, -S) / kDE2RA + 360.0, 360.0);
    return la;
}

LookAngles LookAnglesEci(const GeoPos& obs, const EciState& satEci, double jd)
{
    GeoPos satGeo = EciToGeo(satEci, jd);
    return LookAnglesTo(obs, satGeo);
}

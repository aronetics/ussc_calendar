#include "PassPredictor.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

static const double kMINPDAY = 1440.0;
static const double kSECPDAY = 86400.0;

// Compute elevation (degrees) of satellite at given JD
static double ElevAtJd(const TleData& tle, const GroundStation& gs, double jd)
{
    double tsince = (jd - tle.epochJd) * kMINPDAY;
    EciState eci = Sgp4Propagate(tle, tsince);
    if (eci.x == 0.0 && eci.y == 0.0 && eci.z == 0.0) return -90.0;
    GeoPos obs;
    obs.latDeg = gs.latDeg;
    obs.lonDeg = gs.lonDeg;
    obs.altKm  = gs.altKm;
    LookAngles la = LookAnglesEci(obs, eci, jd);
    return la.elDeg;
}

static LookAngles LookAtJd(const TleData& tle, const GroundStation& gs, double jd)
{
    double tsince = (jd - tle.epochJd) * kMINPDAY;
    EciState eci = Sgp4Propagate(tle, tsince);
    GeoPos obs;
    obs.latDeg = gs.latDeg;
    obs.lonDeg = gs.lonDeg;
    obs.altKm  = gs.altKm;
    return LookAnglesEci(obs, eci, jd);
}

// Refine AOS/LOS crossing by bisection
static double RefineCrossing(const TleData& tle, const GroundStation& gs,
                               double jdLo, double jdHi, double minEl,
                               double fineStepSec)
{
    double secStep = fineStepSec / kSECPDAY;
    for (int i = 0; i < 20; ++i) {
        double jdMid = 0.5 * (jdLo + jdHi);
        double el = ElevAtJd(tle, gs, jdMid);
        if (el >= minEl) jdHi = jdMid;
        else             jdLo = jdMid;
        if ((jdHi - jdLo) < secStep) break;
    }
    return 0.5 * (jdLo + jdHi);
}

static wxDateTime JdToWx(double jd)
{
    // JD → Unix time (seconds since 1970-01-01 00:00:00 UTC)
    double unixSec = (jd - 2440587.5) * kSECPDAY;
    wxDateTime dt;
    dt.Set((time_t)unixSec);
    return dt;
}

std::vector<SatPass> PredictPasses(
    const std::vector<Satellite>&     satellites,
    const std::vector<GroundStation>& stations,
    const TleStore& tleStore,
    double startJd,
    double lookaheadDays,
    double coarseStepSec,
    double fineStepSec)
{
    std::vector<SatPass> passes;

    double coarseJdStep = coarseStepSec / kSECPDAY;
    double endJd        = startJd + lookaheadDays;

    for (const auto& sat : satellites) {
        if (sat.noradId.empty()) continue;
        int norad = std::atoi(sat.noradId.c_str());
        if (norad <= 0) continue;

        const TleEntry* entry = tleStore.Get(norad);
        if (!entry || !entry->parsed.valid) continue;
        const TleData& tle = entry->parsed;

        // Skip GEO — it's always in view above ~5°, no discrete passes
        if (sat.orbitType == "GEO") continue;

        for (const auto& gs : stations) {
            if (!gs.active) continue;

            double minEl = gs.minElDeg;

            // Coarse scan
            bool wasAbove = false;
            double riseJd = 0.0;
            double maxEl  = -90.0;
            double tcaJd  = 0.0;
            double prevJd = startJd;

            for (double jd = startJd; jd <= endJd; jd += coarseJdStep) {
                double el = ElevAtJd(tle, gs, jd);
                bool above = (el >= minEl);

                if (above && !wasAbove) {
                    // Refine AOS between prevJd and jd
                    riseJd = RefineCrossing(tle, gs, prevJd, jd, minEl, fineStepSec);
                    maxEl  = el;
                    tcaJd  = jd;
                }
                if (above) {
                    if (el > maxEl) { maxEl = el; tcaJd = jd; }
                }
                if (!above && wasAbove) {
                    // Refine LOS between prevJd and jd
                    double setJd = RefineCrossing(tle, gs, prevJd, jd, minEl, fineStepSec);

                    SatPass p;
                    p.noradId          = norad;
                    p.satName          = sat.name;
                    p.satId            = sat.id;
                    p.groundStationId  = gs.id;
                    p.groundStationName= gs.name;
                    p.aos              = JdToWx(riseJd);
                    p.tca              = JdToWx(tcaJd);
                    p.los              = JdToWx(setJd);
                    p.maxElDeg         = maxEl;
                    p.aosDeg           = ElevAtJd(tle, gs, riseJd);
                    LookAngles tcaLa   = LookAtJd(tle, gs, tcaJd);
                    p.tcaAzDeg         = tcaLa.azDeg;
                    passes.push_back(p);

                    maxEl = -90.0; tcaJd = 0.0; riseJd = 0.0;
                }

                wasAbove = above;
                prevJd   = jd;
            }
        }
    }

    std::sort(passes.begin(), passes.end(),
        [](const SatPass& a, const SatPass& b) {
            return a.aos.GetTicks() < b.aos.GetTicks();
        });

    return passes;
}

CalendarEvent PassToEvent(const SatPass& pass, const wxColour& color)
{
    CalendarEvent ev;
    ev.start = pass.aos;
    ev.end   = pass.los;
    ev.color = color;

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "SAT PASS: %s via %s  |  MaxEl: %.1f°  TCA Az: %.1f°",
        pass.satName.c_str(),
        pass.groundStationName.c_str(),
        pass.maxElDeg,
        pass.tcaAzDeg);
    ev.title = buf;

    std::snprintf(buf, sizeof(buf),
        "NORAD: %d | AOS El: %.1f° | Max El: %.1f° @ Az %.1f°\n"
        "Ground Station: %s",
        pass.noradId,
        pass.aosDeg,
        pass.maxElDeg,
        pass.tcaAzDeg,
        pass.groundStationName.c_str());
    ev.description = buf;

    // Tag so the app knows it's a generated satellite pass
    ev.location = "sat_pass:" + pass.satId + ":" + pass.groundStationId;

    return ev;
}

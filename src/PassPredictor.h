#pragma once
#include "Sgp4.h"
#include "GroundStationStore.h"
#include "SatelliteStore.h"
#include "TleStore.h"
#include "CalendarEvent.h"
#include <vector>
#include <string>

struct SatPass {
    int         noradId   = 0;
    std::string satName;
    std::string satId;         // internal Satellite.id
    std::string groundStationId;
    std::string groundStationName;
    wxDateTime  aos;           // acquisition of signal
    wxDateTime  tca;           // time of closest approach (max elevation)
    wxDateTime  los;           // loss of signal
    double      maxElDeg  = 0; // max elevation during pass
    double      aosDeg    = 0; // elevation at AOS
    double      tcaAzDeg  = 0; // azimuth at TCA
};

// Predict passes for all satellites (that have TLEs) across all active ground
// stations, for the next lookaheadDays from 'startJd'.
// Step granularity: coarseStepSec for search (default 30s), fineStepSec for
// AOS/LOS refinement (default 2s).
std::vector<SatPass> PredictPasses(
    const std::vector<Satellite>&  satellites,
    const std::vector<GroundStation>& stations,
    const TleStore& tleStore,
    double startJd,
    double lookaheadDays = 3.0,
    double coarseStepSec = 30.0,
    double fineStepSec   = 2.0);

// Convert a SatPass to a CalendarEvent tagged as a satellite pass
CalendarEvent PassToEvent(const SatPass& pass, const wxColour& color);

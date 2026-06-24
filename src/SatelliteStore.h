#pragma once
#include <string>
#include <vector>
#include <wx/datetime.h>

class LanClient;

struct CommBand {
    std::string name;
    std::string bandType;   // "UHF","VHF","L","S","C","X","Ku","K","Ka","EHF","SHF"
    double      freqMHz = 0.0;
    bool        uplink  = true;
};

enum class SatStatus { Operational, Degraded, Decommissioned };

struct Satellite {
    std::string id;
    std::string name;
    std::string noradId;
    std::string orbitType;    // "LEO","MEO","GEO","HEO","SSO"
    SatStatus   status      = SatStatus::Operational;
    wxDateTime  launchDate;
    bool        launchKnown = false;
    std::vector<CommBand> bands;
};

class SatelliteStore {
public:
    SatelliteStore();
    void Load();
    void Save() const;

    void SetLanClient(LanClient* c) { client_ = c; }

    std::vector<Satellite>&       Satellites()       { return sats_; }
    const std::vector<Satellite>& Satellites() const { return sats_; }

    void Add(const Satellite& s);
    void Update(const Satellite& s);
    void Remove(const std::string& id);

private:
    std::vector<Satellite> sats_;
    std::string            path_;
    LanClient*             client_ = nullptr;
};

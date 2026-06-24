#pragma once
#include <string>
#include <vector>

class LanClient;

struct GroundStation {
    std::string id;
    std::string name;
    double latDeg    = 0.0;   // N positive
    double lonDeg    = 0.0;   // E positive
    double altKm     = 0.0;   // above WGS84 ellipsoid (km)
    double minElDeg  = 5.0;   // minimum elevation mask (degrees)
    bool   active    = true;
};

class GroundStationStore {
public:
    GroundStationStore();
    void Load();
    void Save() const;

    void SetLanClient(LanClient* c) { client_ = c; }

    std::vector<GroundStation>&       Stations()       { return stations_; }
    const std::vector<GroundStation>& Stations() const { return stations_; }

    void Add(const GroundStation& gs);
    void Update(const GroundStation& gs);
    void Remove(const std::string& id);

private:
    std::vector<GroundStation> stations_;
    std::string                path_;
    LanClient*                 client_ = nullptr;
};

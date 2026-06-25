#pragma once
#include "Sgp4.h"
#include <string>
#include <vector>
#include <map>

struct TleEntry {
    int         noradId   = 0;
    std::string name;
    std::string line1;
    std::string line2;
    double      fetchedJd = 0.0;  // Julian date when fetched
    TleData     parsed;
};

class TleStore {
public:
    TleStore();
    void Load();
    void Save() const;

    // Fetch TLE from Celestrak for given NORAD IDs (comma-separated progress via callback)
    // Returns number of TLEs successfully fetched
    int FetchFromCelestrak(const std::vector<int>& noradIds,
                           std::string& errorMsg);

    // Get TLE for a NORAD ID (nullptr if not cached)
    const TleEntry* Get(int noradId) const;

    const std::map<int, TleEntry>& All() const { return cache_; }

    void PurgeOlderThan(double maxAgeDays);

private:
    std::map<int, TleEntry> cache_;  // keyed by NORAD ID
    std::string             path_;
};

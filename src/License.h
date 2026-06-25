#pragma once
#include <string>

// License types gating single-instance vs. multi-user LAN server capability.
// Key format: PREFIX-XXXXX-XXXXX-CCCCC  (19 chars with dashes)
// PREFIX = "SGL" (single), "MLT" (multi), "TRL" (trial)
// CCCCC = 5-char hex CRC of first 14 chars

enum class LicenseType { None, Trial, SingleUser, MultiUser };

struct LicenseInfo {
    LicenseType type  = LicenseType::None;
    std::string key;
    bool        valid = false;
    std::string message;   // human-readable status
};

class License {
public:
    static LicenseInfo Load();
    static bool        Save(const std::string& key);
    static LicenseInfo Validate(const std::string& key);

    static bool AllowsLanServer(const LicenseInfo& li);

private:
    static std::string Path();
    static unsigned    Crc(const std::string& s);
};

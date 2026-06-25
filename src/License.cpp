#include "License.h"
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdio>

// ── CRC ─────────────────────────────────────────────────────────────────────

unsigned License::Crc(const std::string& s)
{
    unsigned crc = 0xFFFFu;
    for (unsigned char c : s) {
        crc ^= (unsigned)c << 8;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 0x8000u) ? (crc << 1) ^ 0x1021u : (crc << 1);
    }
    return crc & 0xFFFFu;
}

// ── Path ────────────────────────────────────────────────────────────────────

std::string License::Path()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    if (!wxDirExists(dir)) wxMkdir(dir);
    return std::string((dir + "/license.key").mb_str());
}

// ── Validate ────────────────────────────────────────────────────────────────

LicenseInfo License::Validate(const std::string& rawKey)
{
    LicenseInfo li;
    li.key = rawKey;

    // Normalize: uppercase, strip whitespace
    std::string k;
    for (char c : rawKey)
        if (!std::isspace((unsigned char)c)) k += (char)std::toupper((unsigned char)c);

    // Expected: PREFIX-XXXXX-XXXXX-CCCCC (19 chars)
    if (k.size() != 19 || k[3] != '-' || k[9] != '-' || k[15] != '-') {
        li.message = "Invalid key format (expected PREFIX-XXXXX-XXXXX-CCCCC)";
        return li;
    }

    std::string prefix  = k.substr(0, 3);
    std::string data14  = k.substr(0, 15);  // first 15 chars before last dash
    std::string crcPart = k.substr(16);     // 3 chars after last dash... wait

    // Layout: [0-2]=prefix [3]=dash [4-8]=group1 [9]=dash [10-14]=group2 [15]=dash [16-20]=crc
    // data for crc = everything before the last '-'
    std::string dataForCrc = k.substr(0, 15);
    unsigned expected = Crc(dataForCrc);
    char expectedHex[8];
    std::snprintf(expectedHex, sizeof(expectedHex), "%05X", expected & 0xFFFFFu);
    std::string expectedStr(expectedHex);

    if (k.substr(16) != expectedStr) {
        li.message = "Invalid license key (checksum mismatch)";
        return li;
    }

    if      (prefix == "SGL") { li.type = LicenseType::SingleUser; li.message = "Single-User License"; }
    else if (prefix == "MLT") { li.type = LicenseType::MultiUser;  li.message = "Multi-User LAN License"; }
    else if (prefix == "TRL") { li.type = LicenseType::Trial;      li.message = "Trial License (30 days)"; }
    else { li.message = "Unknown license type"; return li; }

    li.valid = true;
    return li;
}

// ── Load / Save ─────────────────────────────────────────────────────────────

LicenseInfo License::Load()
{
    std::ifstream f(Path());
    if (!f.is_open()) {
        LicenseInfo li;
        li.message = "No license installed";
        return li;
    }
    std::string key;
    std::getline(f, key);
    if (!key.empty() && key.back() == '\r') key.pop_back();
    return Validate(key);
}

bool License::Save(const std::string& key)
{
    std::ofstream f(Path());
    if (!f.is_open()) return false;
    f << key << "\n";
    return true;
}

bool License::AllowsLanServer(const LicenseInfo& li)
{
    return li.valid && (li.type == LicenseType::MultiUser || li.type == LicenseType::Trial);
}

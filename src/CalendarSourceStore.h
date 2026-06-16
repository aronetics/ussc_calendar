#pragma once
#include <wx/colour.h>
#include <string>
#include <vector>
#include <functional>

// Operational alliance a calendar source belongs to
enum class Alliance { CSpO, NATO, INDOPACOM };

struct CalendarSource {
    std::string id;        // e.g. "us", "uk", "australia"
    std::string name;      // e.g. "United States"
    Alliance    alliance;
    wxColour    color;
    bool        visible = true;
};

class CalendarSourceStore {
public:
    CalendarSourceStore();
    // Sources are fixed (not user-editable); only visibility persists.
    void LoadVisibility();
    void SaveVisibility() const;

    const std::vector<CalendarSource>& Sources() const { return sources_; }
    std::vector<CalendarSource>&       Sources()       { return sources_; }

    const CalendarSource* Find(const std::string& id) const;
    void SetVisible(const std::string& id, bool vis);
    bool IsVisible(const std::string& id) const;

    // Returns display-name for an alliance
    static const char* AllianceName(Alliance a);

    std::function<void()> OnVisibilityChanged;

private:
    std::vector<CalendarSource> sources_;
    std::string                 visPath_;   // JSON file with {id:bool, ...}
    void InitSources();
};

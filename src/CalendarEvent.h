#pragma once
#include <string>
#include <vector>
#include <wx/datetime.h>
#include <wx/colour.h>

enum class RecurFreq { None, Daily, Weekly, Monthly, Yearly };

struct RecurrenceRule {
    RecurFreq  freq     = RecurFreq::None;
    int        interval = 1;     // every N periods
    wxDateTime until;            // invalid = no end date
    int        count    = 0;     // 0 = no count limit
};

// Google Calendar colour palette as inline functions (avoids static init issues)
inline wxColour GCTomato()     { return wxColour(213,  0,  0);   }
inline wxColour GCFlamingo()   { return wxColour(230,124,115);   }
inline wxColour GCTangerine()  { return wxColour(244, 81, 30);   }
inline wxColour GCBanana()     { return wxColour(246,191, 38);   }
inline wxColour GCSage()       { return wxColour( 51,182,121);   }
inline wxColour GCBasil()      { return wxColour( 11,128, 67);   }
inline wxColour GCPeacock()    { return wxColour(  3,155,229);   }
inline wxColour GCBlueberry()  { return wxColour( 63, 81,181);   }
inline wxColour GCLavender()   { return wxColour(121,134,203);   }
inline wxColour GCGrape()      { return wxColour(142, 36,170);   }
inline wxColour GCGraphite()   { return wxColour( 97, 97, 97);   }
inline wxColour GCDefault()    { return wxColour( 26,115,232);   }  // Google blue

struct CalendarEvent {
    std::string id;
    std::string title;
    std::string description;
    std::string location;
    wxDateTime  start;
    wxDateTime  end;
    bool        allDay = false;
    wxColour    color;
    std::vector<std::string> participants;  // user IDs
    std::string calendarSource;             // CalendarSource id (e.g. "us", "uk")
    RecurrenceRule recurrence;

    CalendarEvent() : color(GCDefault()) {}

    bool OccursOnDate(const wxDateTime& d) const;
    bool StartsOnDate(const wxDateTime& d) const;
};

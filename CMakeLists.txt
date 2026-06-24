cmake_minimum_required(VERSION 3.20)
project(ussc_calendar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Version ───────────────────────────────────────────────────────────────────
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_VERSION)
    set(GIT_VERSION "dev")
endif()

# ── wxWidgets ─────────────────────────────────────────────────────────────────
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/AllianceReportDialog.cpp
    src/CalendarConnector.cpp
    src/CalendarFrame.cpp
    src/CalendarSourcePanel.cpp
    src/CalendarSourceStore.cpp
    src/ConflictDetector.cpp
    src/ConnectorDialog.cpp
    src/DayView.cpp
    src/EventDialog.cpp
    src/EventStore.cpp
    src/EwsConnector.cpp
    src/GraphApiConnector.cpp
    src/GroundStationStore.cpp
    src/IcsConnector.cpp
    src/LoginPanel.cpp
    src/MiniCalendar.cpp
    src/MonthView.cpp
    src/NLEventDialog.cpp
    src/PassPredictor.cpp
    src/PassPredictPanel.cpp
    src/SatellitePanel.cpp
    src/SatelliteStore.cpp
    src/Sgp4.cpp
    src/OutlookComConnector.cpp
    src/TleStore.cpp
    src/UserPanel.cpp
    src/UserStore.cpp
    src/WeekView.cpp
    src/main.cpp
)

if(WIN32)
    add_executable(ussc_calendar WIN32 ${SOURCES} ussc_calendar.rc)
else()
    add_executable(ussc_calendar ${SOURCES})
endif()

target_include_directories(ussc_calendar PRIVATE src)
target_compile_definitions(ussc_calendar PRIVATE APP_VERSION="${GIT_VERSION}")
target_link_libraries(ussc_calendar PRIVATE ${wxWidgets_LIBRARIES})

if(WIN32)
    target_link_libraries(ussc_calendar PRIVATE bcrypt ole32 oleaut32 winhttp)
    # The .rc file embeds our manifest; suppress the linker's auto-generated one
    target_link_options(ussc_calendar PRIVATE /MANIFEST:NO)
endif()

install(TARGETS ussc_calendar DESTINATION bin)

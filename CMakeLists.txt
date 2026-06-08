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
    src/CalendarFrame.cpp
    src/DayView.cpp
    src/EventDialog.cpp
    src/EventStore.cpp
    src/LoginPanel.cpp
    src/MiniCalendar.cpp
    src/MonthView.cpp
    src/WeekView.cpp
    src/main.cpp
)

if(WIN32)
    add_executable(ussc_calendar WIN32 ${SOURCES})
else()
    add_executable(ussc_calendar ${SOURCES})
endif()

target_include_directories(ussc_calendar PRIVATE src)
target_compile_definitions(ussc_calendar PRIVATE APP_VERSION="${GIT_VERSION}")
target_link_libraries(ussc_calendar PRIVATE ${wxWidgets_LIBRARIES})

if(WIN32)
    target_link_libraries(ussc_calendar PRIVATE bcrypt)
endif()

install(TARGETS ussc_calendar DESTINATION bin)

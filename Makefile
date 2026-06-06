CXX      := g++
WX_FLAGS := $(shell wx-config --cxxflags)
WX_LIBS  := $(shell wx-config --libs all)

# GIT_VERSION is injected at compile time; CI/CD overrides with CI_COMMIT_TAG
GIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")

# CXXFLAGS_EXTRA: append extra flags from the command line without losing defaults
# e.g.  make CXXFLAGS_EXTRA="-Wextra -Wshadow"
CXXFLAGS := $(WX_FLAGS) -std=c++17 -Wall -O2 -Isrc \
            -DAPP_VERSION=\"$(GIT_VERSION)\" \
            $(CXXFLAGS_EXTRA)
LDFLAGS  := $(WX_LIBS)

TARGET  := ussc_calendar
SRCDIR  := src
OBJDIR  := obj
BINDIR  := bin

SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: all clean run

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR) $(BINDIR):
	mkdir -p $@

run: all
	$(BINDIR)/$(TARGET)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

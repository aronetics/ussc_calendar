CXX      := g++
WX_FLAGS := $(shell wx-config --cxxflags)
WX_LIBS  := $(shell wx-config --libs all)
CXXFLAGS := $(WX_FLAGS) -std=c++17 -Wall -O2 -Isrc
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

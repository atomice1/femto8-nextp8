ifeq ($(OS),Windows_NT)
  PLATFORM ?= windows
else
  UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
  ifeq ($(UNAME_S),Linux)
    PLATFORM ?= linux
  else ifeq ($(UNAME_S),Darwin)
    PLATFORM ?= mac
  else ifneq (,$(findstring MINGW,$(UNAME_S)))
    PLATFORM ?= windows
  else ifneq (,$(findstring MSYS,$(UNAME_S)))
    PLATFORM ?= windows
  else
    PLATFORM ?= linux
  endif
endif

PLATFORM_MK := Makefile.$(PLATFORM)
ifeq (,$(wildcard $(PLATFORM_MK)))
  $(error No platform file '$(PLATFORM_MK)' found)
endif

include $(PLATFORM_MK)

BUILD_DIR ?= build-$(PLATFORM)
TARGET_NAME := femto8
TARGET := $(BUILD_DIR)/$(TARGET_NAME)

INCFLAGS += -Isrc -Isrc/data -Isrc/lua -Isrc/lodepng -Isrc/lexaloffle
DEFINES += -DLODEPNG_NO_COMPILE_ENCODER -DLODEPNG_NO_COMPILE_DISK -DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS -DLODEPNG_NO_COLOR_CONVERT

# NOTE: SDL_CFLAGS is provided by the platform Makefile (pkg-config/sdl-config)
CFLAGS   += -Wall $(DEFINES) $(fpic) $(INCFLAGS) $(SDL_CFLAGS) -g -O3 -Wall -Werror
CXXFLAGS += $(CFLAGS) -fno-threadsafe-statics

LINKER ?= $(CC)

# Source directories
SRC_DIRS := src src/lua src/data src/lodepng src/lexaloffle src/wifi

# Create build directories
$(shell mkdir -p $(BUILD_DIR)/lua $(BUILD_DIR)/lexaloffle $(BUILD_DIR)/lodepng $(BUILD_DIR)/wifi)
$(shell mkdir -p $(BUILD_DIR)/unittests/emu $(BUILD_DIR)/unittests/dialog $(BUILD_DIR)/unittests/editor)

# Collect all source files
SOURCES_LUA := $(wildcard src/lua/*.c) src/p8_lua.c
SOURCES_C := $(filter-out $(SOURCES_LUA),$(wildcard $(addsuffix /*.c, $(SRC_DIRS))))
SOURCES_CXX := $(wildcard $(addsuffix /*.cpp, $(SRC_DIRS)))

# Generate object file paths
OBJECTS_C := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES_C))
OBJECTS_CXX := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES_CXX))
OBJECTS_LUA := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES_LUA))

OBJECTS := $(OBJECTS_C) $(OBJECTS_CXX) $(OBJECTS_LUA)

all: $(TARGET) unittests

$(BUILD_DIR)/unittests/CMakeCache.txt:
	@mkdir -p $(BUILD_DIR)/unittests
	cmake -S unittests -B $(BUILD_DIR)/unittests -DCMAKE_BUILD_TYPE=Debug

unittests: $(BUILD_DIR)/unittests/CMakeCache.txt
	cmake --build $(BUILD_DIR)/unittests

$(TARGET): $(OBJECTS)
	$(LINKER) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

$(OBJECTS_C): $(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
	$(CC) -MM $(CFLAGS) -MT $@ -MF $(BUILD_DIR)/$*.d $<

$(OBJECTS_CXX): $(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
	$(CXX) -MM $(CXXFLAGS) -MT $@ -MF $(BUILD_DIR)/$*.d $<

$(OBJECTS_LUA): $(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
	$(CC) -MM $(CFLAGS) -MT $@ -MF $(BUILD_DIR)/$*.d $<

clean::
	rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET)
	rm -rf $(BUILD_DIR)/unittests

test: unittests
	@echo "Running tests..."
	cd $(BUILD_DIR)/unittests && ctest --output-on-failure

.PHONY: clean test

-include $(OBJECTS:.o=.d)
-include Makefile.$(PLATFORM).post

# SPDX-License-Identifier: GPL-2.0-only
DEVKITPRO ?= /opt/devkitpro
LIBOUNCE ?= $(DEVKITPRO)/libounce
# The Switch 2 CPU flags (SWITCH2_CPU).
include $(LIBOUNCE)/switch2_cpu.mk
PREFIX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
OUT := build
SOURCES := $(wildcard source/*.cpp)
OBJECTS := $(patsubst source/%.cpp,$(OUT)/%.o,$(SOURCES))
# Code that HEN shares with the bootstrap.
OBJECTS += $(OUT)/policy_package.o $(OUT)/sd_writer.o
ARCH := $(SWITCH2_CPU) -mtp=soft -fPIE
CXXFLAGS := $(ARCH) -std=gnu++20 -O2 -Wall -Wextra -Werror \
    -Wno-missing-field-initializers -fno-exceptions -fno-rtti \
    -ffunction-sections -fdata-sections -MMD -MP \
    -D__SWITCH__ -D__SWITCH2__ \
    -I$(LIBOUNCE)/include -I$(DEVKITPRO)/libnx/include -I../libkaihen/include -Isource
LDFLAGS := $(ARCH) -specs=$(OUT)/switch.specs -Wl,--gc-sections \
    -Wl,-Map,$(OUT)/hen.map

all: $(OUT)/main

$(OUT):
	mkdir -p $@

$(OUT)/switch.specs: $(LIBOUNCE)/switch.specs | $(OUT)
	sed 's|%:getenv(DEVKITPRO /libnx/switch.ld)|$(abspath $(LIBOUNCE))/switch.ld|' $< > $@

$(OUT)/%.o: source/%.cpp | $(OUT)
	$(PREFIX)g++ $(CXXFLAGS) -c $< -o $@

$(OUT)/%.o: ../common/%.cpp | $(OUT)
	$(PREFIX)g++ $(CXXFLAGS) -c $< -o $@

# HEN links libounce without its allocator (-lounce_core) and keeps
# newlib's malloc: mimalloc's fixed costs do not fit a small heap.
$(OUT)/hen.elf: $(OBJECTS) $(OUT)/switch.specs $(LIBOUNCE)/lib/libounce_core.a
	$(PREFIX)g++ $(LDFLAGS) $(OBJECTS) -L$(LIBOUNCE)/lib -lounce_core -o $@

$(OUT)/main: $(OUT)/hen.elf
	$(DEVKITPRO)/tools/bin/elf2nso $< $@

clean:
	rm -rf $(OUT)

-include $(OBJECTS:.o=.d)
.PHONY: all clean

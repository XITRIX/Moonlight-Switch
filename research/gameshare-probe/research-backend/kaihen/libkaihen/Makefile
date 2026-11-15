# SPDX-License-Identifier: ISC
# libkaihen: kaiHEN's homebrew API, which the Switch 2 devkit ships.
#   include/kaihen.h, include/kaihen/   the API headers
#   build/kaihen-jit.o      the JIT provider for libounce's Jit API (-lkaihen)
#   build/libkaihen.a       a linker script that names kaihen-jit.o
#   build/libkaihen_plugin.a  the plugin runtime (-lkaihen_plugin)
#
#   make                          with an installed devkit ($DEVKITPRO/libounce)
#   make LIBOUNCE=DIR          with another libounce installation or build
#   make install PORTLIB=DIR      install into a portlib folder (default
#                                 $DEVKITPRO/portlibs/switch2)
DEVKITPRO ?= /opt/devkitpro
LIBOUNCE ?= $(DEVKITPRO)/libounce
PORTLIB ?= $(DEVKITPRO)/portlibs/switch2
# The Switch 2 CPU flags (SWITCH2_CPU).
include $(LIBOUNCE)/switch2_cpu.mk
TOOLS := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
OUT := build
COMMON_FLAGS := -O2 -Wall -Wextra -Werror $(SWITCH2_CPU) -mtp=soft -fPIE \
    -ffunction-sections -fdata-sections -D__SWITCH__ -D__SWITCH2__ \
    -I$(LIBOUNCE)/include -Iinclude
CFLAGS := -std=gnu11 $(COMMON_FLAGS)
CXXFLAGS := -std=gnu++20 -Wno-missing-field-initializers -fno-exceptions -fno-rtti \
    $(COMMON_FLAGS)

all: $(OUT)/kaihen-jit.o $(OUT)/libkaihen.a $(OUT)/libkaihen_plugin.a

$(OUT):
	mkdir -p $@

# An object, not an archive member: libounce refers to the JIT hooks weakly,
# and a weak reference does not pull a member out of an archive. The linker
# script libkaihen.a names the object, so -lkaihen links it.
$(OUT)/kaihen-jit.o: source/jit.c | $(OUT)
	$(TOOLS)gcc $(CFLAGS) -c $< -o $@

$(OUT)/libkaihen.a: | $(OUT)
	echo 'INPUT(kaihen-jit.o)' > $@

$(OUT)/plugin_%.o: source/plugin_%.cpp | $(OUT)
	$(TOOLS)g++ $(CXXFLAGS) -c $< -o $@

$(OUT)/libkaihen_plugin.a: $(OUT)/plugin_runtime.o $(OUT)/plugin_server.o
	rm -f $@
	$(TOOLS)ar rcs $@ $^

install: all
	mkdir -p $(PORTLIB)/include $(PORTLIB)/lib $(PORTLIB)/share/licenses/kaihen
	cp -r include/kaihen.h include/kaihen $(PORTLIB)/include/
	cp $(OUT)/kaihen-jit.o $(OUT)/libkaihen.a $(OUT)/libkaihen_plugin.a $(PORTLIB)/lib/
	cp LICENSE $(PORTLIB)/share/licenses/kaihen/kaihen-ISC.txt

clean:
	rm -rf $(OUT)

.PHONY: all install clean

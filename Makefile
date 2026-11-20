PROJECT := neuronte-sdk
BUILD_DIR ?= build
PREFIX ?= /usr/local
DESTDIR ?=

CC ?= cc
AR ?= ar
INSTALL ?= install
PYTHON ?= python3

CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude -I$(BUILD_DIR)
WARNINGS := \
	-Wall -Wextra -Wpedantic -Wconversion -Wshadow \
	-Wstrict-prototypes -Wmissing-prototypes -Wformat=2
CFLAGS ?= -O2 -g
override CFLAGS += -std=c11 -fPIC $(WARNINGS)
LDLIBS := -ldl

# The terminal opener calls neuronte_* and needs a Neuronte checkout that
# opens tiled bundles. An explicit NEURONTE= is kept. Command-line CPPFLAGS
# must not be the way to add these flags: CPPFLAGS += above keeps -Iinclude,
# and these two variables carry the Neuronte header and the optional
# -DNSDK_TERMINAL_EXTRA_DIR / -DNSDK_HAVE_UNTRAINED defines.
NEURONTE ?= $(HOME)/projects/neuronte
ifeq ($(origin NEURONTE), file)
ifeq ($(shell grep -q 'neuronte_set_untrained' '$(NEURONTE)/include/neuronte.h' 2>/dev/null && echo yes),)
DISCOVERED_NEURONTE := $(shell $(PYTHON) scripts/discover_neuronte_src.py 2>/dev/null)
ifneq ($(DISCOVERED_NEURONTE),)
NEURONTE := $(DISCOVERED_NEURONTE)
endif
endif
endif
NEURONTE_INCLUDE ?= $(NEURONTE)/include
NSDK_EXTRA_CPPFLAGS ?=
NSDK_TERMINAL_EXTRA_DIR ?=
NSDK_TERMINAL_CPPFLAGS := -I$(NEURONTE_INCLUDE)
ifeq ($(shell grep -q 'neuronte_set_untrained' '$(NEURONTE_INCLUDE)/neuronte.h' 2>/dev/null && echo yes),yes)
NSDK_TERMINAL_CPPFLAGS += -DNSDK_HAVE_UNTRAINED
endif
ifneq ($(NSDK_TERMINAL_EXTRA_DIR),)
NSDK_TERMINAL_DIR_FLAG := -DNSDK_TERMINAL_EXTRA_DIR='"$(NSDK_TERMINAL_EXTRA_DIR)"'
endif

LIB_OBJS := $(BUILD_DIR)/neuronte_sdk.o $(BUILD_DIR)/nsdk_terminal.o
STATIC_LIB := $(BUILD_DIR)/lib$(PROJECT).a
SHARED_LIB := $(BUILD_DIR)/lib$(PROJECT).so
TEST_BIN := $(BUILD_DIR)/test-sdk
EXAMPLE_BIN := $(BUILD_DIR)/grid

.PHONY: all clean example install python-check test

all: $(STATIC_LIB) $(SHARED_LIB) $(EXAMPLE_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/neuronte_sdk.o: src/neuronte_sdk.c include/neuronte_sdk.h \
		scripts/discover_neuronte.py | $(BUILD_DIR)
	$(PYTHON) scripts/discover_neuronte.py $(BUILD_DIR)/nsdk_defaults.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: nsdk_flags_force
$(BUILD_DIR)/nsdk_terminal.flags: nsdk_flags_force | $(BUILD_DIR)
	$(file >$(BUILD_DIR)/nsdk_terminal.flags.tmp,$(strip $(NSDK_TERMINAL_CPPFLAGS) $(NSDK_TERMINAL_DIR_FLAG) $(NSDK_EXTRA_CPPFLAGS)))
	@cmp -s $(BUILD_DIR)/nsdk_terminal.flags.tmp $@ || cp $(BUILD_DIR)/nsdk_terminal.flags.tmp $@
	@rm -f $(BUILD_DIR)/nsdk_terminal.flags.tmp

$(BUILD_DIR)/nsdk_terminal.o: src/nsdk_terminal.c include/neuronte_sdk_terminal.h \
		$(BUILD_DIR)/nsdk_terminal.flags
	$(CC) $(CPPFLAGS) $(NSDK_TERMINAL_CPPFLAGS) $(NSDK_TERMINAL_DIR_FLAG) $(NSDK_EXTRA_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(LIB_OBJS)
	$(CC) -shared $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST_BIN): tests/test_sdk.c $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(EXAMPLE_BIN): examples/grid.c $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) -o $@

test: $(TEST_BIN)
	$(TEST_BIN)

example: $(EXAMPLE_BIN)

python-check:
	$(MAKE) -C python check

install: all
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib
	$(INSTALL) -m 0644 include/neuronte_sdk.h include/neuronte_sdk_terminal.h \
		$(DESTDIR)$(PREFIX)/include/
	$(INSTALL) -m 0644 $(STATIC_LIB) $(SHARED_LIB) $(DESTDIR)$(PREFIX)/lib/

clean:
	rm -rf $(BUILD_DIR) grid.ppm
	$(MAKE) -C python clean

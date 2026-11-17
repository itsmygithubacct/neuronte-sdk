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
LDLIBS := -ldl -lm

LIB_OBJS := $(BUILD_DIR)/neuronte_sdk.o $(BUILD_DIR)/neuronte_sdk_canvas.o
STATIC_LIB := $(BUILD_DIR)/lib$(PROJECT).a
SHARED_LIB := $(BUILD_DIR)/lib$(PROJECT).so
TEST_BIN := $(BUILD_DIR)/test-sdk
CANVAS_TEST_BIN := $(BUILD_DIR)/test-canvas
EXAMPLE_BIN := $(BUILD_DIR)/grid

.PHONY: all canvas-reference clean example install python-check test test-canvas

all: $(STATIC_LIB) $(SHARED_LIB) $(EXAMPLE_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/neuronte_sdk.o: src/neuronte_sdk.c include/neuronte_sdk.h \
		scripts/discover_neuronte.py | $(BUILD_DIR)
	$(PYTHON) scripts/discover_neuronte.py $(BUILD_DIR)/nsdk_defaults.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# The canvas includes nothing of Neuronte, so it needs no discovery step.
$(BUILD_DIR)/neuronte_sdk_canvas.o: src/neuronte_sdk_canvas.c include/neuronte_sdk_canvas.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(LIB_OBJS)
	$(CC) -shared $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST_BIN): tests/test_sdk.c $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(EXAMPLE_BIN): examples/grid.c $(STATIC_LIB) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(STATIC_LIB) $(LDFLAGS) $(LDLIBS) -o $@

# The canvas test links the canvas object alone: it must not need the rest.
$(CANVAS_TEST_BIN): tests/test_canvas.c $(BUILD_DIR)/neuronte_sdk_canvas.o include/neuronte_sdk_canvas.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/neuronte_sdk_canvas.o $(LDFLAGS) -lm -o $@

test-canvas: $(CANVAS_TEST_BIN)
	$(CANVAS_TEST_BIN)

test: $(TEST_BIN) test-canvas
	$(TEST_BIN)

# Print the sums pinned in tests/test_canvas.c from a soft-raster checkout:
# the same test built against soft-raster in place of the SDK's canvas.
canvas-reference: | $(BUILD_DIR)
	@test -n "$(SOFT_RASTER)" || { echo "set SOFT_RASTER to a soft-raster checkout" >&2; exit 2; }
	$(CC) $(CPPFLAGS) -DNSDK_CANVAS_REFERENCE -I$(SOFT_RASTER)/include -O2 -std=c11 \
		tests/test_canvas.c $(SOFT_RASTER)/src/soft_raster.c -lm -o $(BUILD_DIR)/canvas-reference
	$(BUILD_DIR)/canvas-reference

example: $(EXAMPLE_BIN)

python-check:
	$(MAKE) -C python check

install: all
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib
	$(INSTALL) -m 0644 include/neuronte_sdk.h include/neuronte_sdk_canvas.h $(DESTDIR)$(PREFIX)/include/
	$(INSTALL) -m 0644 $(STATIC_LIB) $(SHARED_LIB) $(DESTDIR)$(PREFIX)/lib/
	# The libraries hold code adapted from soft-raster: its notice goes with them.
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/share/doc/neuronte-sdk
	$(INSTALL) -m 0644 LICENSE THIRD-PARTY.md $(DESTDIR)$(PREFIX)/share/doc/neuronte-sdk/

clean:
	rm -rf $(BUILD_DIR) grid.ppm
	$(MAKE) -C python clean

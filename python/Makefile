PYTHON ?= python3
SDK_DIR ?= ..
SDK_LIBRARY := $(abspath $(SDK_DIR)/build/libneuronte-sdk.so)
PYTHON_SOURCES := $(wildcard src/neuronte_sdk/*.py)
TEST_SOURCES := $(wildcard tests/*.py)

.DEFAULT_GOAL := all

all: native compile

native:
	$(MAKE) -C $(SDK_DIR) all

upstream-test:
	$(MAKE) -C $(SDK_DIR) test

compile:
	$(PYTHON) -m py_compile $(PYTHON_SOURCES) $(TEST_SOURCES)

test: native
	PYTHONPATH=src NEURONTE_SDK_LIBRARY=$(SDK_LIBRARY) \
		$(PYTHON) -m unittest discover -s tests -v

check: upstream-test compile test

clean:
	rm -rf build dist src/neuronte_sdk.egg-info src/neuronte_sdk/__pycache__ \
		tests/__pycache__

.PHONY: all native upstream-test compile test check clean

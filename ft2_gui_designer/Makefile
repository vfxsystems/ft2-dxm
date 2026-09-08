BUILD_SCRIPT ?= ../scripts/build-gui-designer.sh
BUILD_DIR ?= ../build-gui-designer
BUILD_PATH := $(abspath $(BUILD_DIR))
BUILD_TYPE ?= Release
JOBS ?= 4

.PHONY: all test run debug clean

all:
	FT2_BUILD_TYPE="$(BUILD_TYPE)" "$(BUILD_SCRIPT)" \
		--build-dir "$(BUILD_PATH)" --jobs "$(JOBS)"

test:
	FT2_BUILD_TYPE="$(BUILD_TYPE)" "$(BUILD_SCRIPT)" \
		--build-dir "$(BUILD_PATH)" --jobs "$(JOBS)" --test

run: all
	"$(BUILD_PATH)/ft2_gui_designer"

debug:
	$(MAKE) BUILD_TYPE=Debug all

clean:
	cmake -E remove_directory "$(BUILD_PATH)"

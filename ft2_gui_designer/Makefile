CMAKE ?= cmake
BUILD_DIR ?= ../build-gui-designer
BUILD_TYPE ?= Release
JOBS ?= 4

.PHONY: all configure test run debug clean

all: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel "$(JOBS)"

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
		-DBUILD_TESTING=ON

test: all
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

run: all
	"$(BUILD_DIR)/ft2_gui_designer"

debug:
	$(MAKE) BUILD_TYPE=Debug all

clean:
	$(CMAKE) -E remove_directory "$(BUILD_DIR)"

BUILD_TYPE := $(if $(BUILD_TYPE),$(BUILD_TYPE),Debug)
.SILENT: all
all: .build
.SILENT: debug
debug: .build

.PHONY: cmake
.SILENT: cmake
cmake:
	if [ ! -d "build" ]; then mkdir "build"; fi
	cd build; cmake -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(realpath $(PWD))

.SILENT: .autogen
.autogen: cmake
	BUILD_TYPE=$(BUILD_TYPE) $(MAKE) -j4 autoshift_autogen -C build
ui: .autogen

.SILENT: .build
.build: cmake
	BUILD_TYPE=$(BUILD_TYPE) $(MAKE) -j4 autoshift -C build

tests: .build-test
	cd build/bin/$(BUILD_TYPE) ; ./unittest

.SILENT: release
release:
	$(eval BUILD_TYPE := Release)
	BUILD_TYPE=$(BUILD_TYPE) $(MAKE) all

# clean:
# 	rm -rf build

.SILENT: .build-test
.build-test: cmake
	BUILD_TYPE=$(BUILD_TYPE) $(MAKE) -j4 unittest -C build


.SILENT: app
app: .build run
rapp: release run


.SILENT: run
run:
	cd build/bin/$(BUILD_TYPE) ; ./autoshift


.SILENT: compdb
compdb:
	if [ ! -d "build" ]; then mkdir "build"; fi
	cd build; PYENV_VERSION=venv compdb list > compile_commands_headers.json

# Host build for the DFRobot_C4001 parser harness.
#
# NOTE: The toolchain's ASan/UBSan runtime shared libraries are not installed
# system-wide (libasan.so / libubsan.so are missing under /usr/lib64). The
# generic GCC builds expect them in the linker search path, so copy real
# implementations into build/asan first (see test/README.md -> "Sanitizer
# runtime"). On a system with a standard libasan/libubsan install this is not
# needed.

CXXFLAGS = -std=c++17 -Wall -Wextra
INC      = -Isrc -Itest/include
SAN      = -fsanitize=address,undefined
ASANLIB  = -Lbuild/asan

OUT = build/radar_test

all: run

build:
	$(CXX) $(CXXFLAGS) $(SAN) $(INC) $(ASANLIB) src/DFRobot_C4001.cpp test/harness.cpp -o $(OUT)

run: build
	if [ -d build/asan ]; then \
	  LD_LIBRARY_PATH=$$(pwd)/build/asan $(OUT); \
	else \
	  $(OUT); \
	fi

clean:
	rm -rf build

.PHONY: all build run clean
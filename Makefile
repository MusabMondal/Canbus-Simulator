CC = cc
CXX = c++
CXXFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c++17
GTEST_DIR ?= build/_deps/googletest-1.17.0
GTEST_FLAGS = -isystem $(GTEST_DIR)/googletest/include -isystem $(GTEST_DIR)/googlemock/include
CFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c99
CPPFLAGS = -Isrc
SOURCES = src/simulator.c src/vehicle_model.c src/can_transport.c
OBJECTS = $(SOURCES:src/%.c=build/%.o)
TARGET = ecu_sim

.PHONY: all run test clean
all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@
build/%.o: src/%.c src/vehicle_model.h src/can_transport.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
run: $(TARGET)
	./$(TARGET)
# Override GTEST_DIR to use an existing GoogleTest 1.17.0 source checkout offline.
$(GTEST_DIR)/.ready:
	@if test -f $(GTEST_DIR)/googlemock/src/gmock-all.cc; then touch $@; else $(MAKE) fetch-googletest; fi

.PHONY: fetch-googletest
fetch-googletest:
	@mkdir -p build/_deps
	curl -fL --retry 2 https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.17.0 -o build/_deps/googletest.tar.gz
	python3 -c 'import hashlib; from pathlib import Path; p=Path("build/_deps/googletest.tar.gz"); assert hashlib.sha256(p.read_bytes()).hexdigest() == "65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c", "GoogleTest archive checksum mismatch"'
	tar -xzf build/_deps/googletest.tar.gz -C build/_deps
	@touch $(GTEST_DIR)/.ready
build/gtest.o: $(GTEST_DIR)/.ready
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -I$(GTEST_DIR)/googletest -I$(GTEST_DIR)/googlemock -pthread -c $(GTEST_DIR)/googletest/src/gtest-all.cc -o $@
build/gmock.o: $(GTEST_DIR)/.ready
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -I$(GTEST_DIR)/googletest -I$(GTEST_DIR)/googlemock -pthread -c $(GTEST_DIR)/googlemock/src/gmock-all.cc -o $@
build/gmock_main.o: $(GTEST_DIR)/.ready
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(GTEST_FLAGS) -pthread -c $(GTEST_DIR)/googlemock/src/gmock_main.cc -o $@
build/test_simulator.o: tests/test_simulator.cpp src/vehicle_model.h src/can_transport.h | $(GTEST_DIR)/.ready
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GTEST_FLAGS) -pthread -c $< -o $@
build/test_simulator: build/test_simulator.o build/vehicle_model.o build/can_transport.o build/gtest.o build/gmock.o build/gmock_main.o
	$(CXX) $(CXXFLAGS) -pthread $^ -o $@
test: $(TARGET) build/test_simulator
	./build/test_simulator
	./$(TARGET) --steps 1000 --fast --quiet --output build/test_telemetry.csv
	python3 tests/check_telemetry.py build/test_telemetry.csv
clean:
	rm -rf build $(TARGET)

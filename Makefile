CC = cc
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
build/test_simulator: tests/test_simulator.c src/vehicle_model.c src/can_transport.c src/vehicle_model.h src/can_transport.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_simulator.c src/vehicle_model.c src/can_transport.c -o $@
test: $(TARGET) build/test_simulator
	./build/test_simulator
	./$(TARGET) --steps 1000 --fast --quiet --output build/test_telemetry.csv
	python3 tests/check_telemetry.py build/test_telemetry.csv
clean:
	rm -rf build $(TARGET)

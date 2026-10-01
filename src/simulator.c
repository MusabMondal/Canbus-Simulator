#define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "vehicle_model.h"
#include "can_transport.h"

static volatile sig_atomic_t running = 1;
static void stop(int signal_number) { (void)signal_number; running = 0; }
static void log_state(FILE *file, const VehicleState *s, unsigned long tick) {
    fprintf(file, "%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", tick * 10,
        s->rpm, s->speed, s->throttle, s->brake_pressure, s->gear,
        s->oil_pressure, s->engine_temp, s->battery_voltage, s->engine_on);
}
int main(int argc, char **argv) {
    unsigned long steps = 0;
    int fast = 0, quiet = 0;
    const char *output = "telemetry_log.csv";
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--fast")) fast = 1;
        else if (!strcmp(argv[i], "--quiet")) quiet = 1;
        else if (!strcmp(argv[i], "--output") && i + 1 < argc) output = argv[++i];
        else if (!strcmp(argv[i], "--steps") && i + 1 < argc) {
            char *end;
            const char *value = argv[++i];
            errno = 0;
            steps = strtoul(value, &end, 10);
            if (errno || *end || end == value || value[0] == '-' || !steps) {
                fprintf(stderr, "--steps requires a positive integer\n"); return 1;
            }
        } else if (!strcmp(argv[i], "--help")) {
            puts("Usage: ecu_sim [--steps N] [--fast] [--quiet] [--output PATH]"); return 0;
        } else { fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]); return 1; }
    }
    FILE *csv = fopen(output, "w");
    if (!csv) { perror(output); return 1; }
    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    VehicleState transmitter = {0}, receiver = {0};
    CANBus bus = {0};
    transmitter.engine_on = 1;
    fputs("Timestamp,RPM,Speed,Throttle,BrakePressure,Gear,OilPressure,EngineTemp,BatteryVoltage,EngineOn\n", csv);
    for (unsigned long tick = 0; running && (!steps || tick < steps); ++tick) {
        if (tick && tick % 100 == 0) transmitter.brake = !transmitter.brake;
        vehicle_update(&transmitter);
        vehicle_update_diagnostics(&transmitter);
        CANFrame frame;
        encode_powertrain_frame(&transmitter, &frame); can_bus_send(&bus, &frame);
        encode_dynamics_frame(&transmitter, &frame); can_bus_send(&bus, &frame);
        encode_sensors_frame(&transmitter, &frame); can_bus_send(&bus, &frame);
        can_bus_arbitrate(&bus);
        can_bus_deliver(&bus, &receiver);
        if (!quiet) vehicle_print(&receiver);
        log_state(csv, &receiver, tick);
        bus.count = 0;
        if (!fast) { struct timespec delay = {0, 10000000}; nanosleep(&delay, NULL); }
    }
    int failed = ferror(csv);
    if (fclose(csv)) failed = 1;
    if (failed) { fprintf(stderr, "Failed to write telemetry\n"); return 1; }
    return 0;
}

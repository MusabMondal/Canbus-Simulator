#include <stdio.h>
#include "can_transport.h"
#include "vehicle_model.h"

uint8_t can_checksum(const uint8_t *data, uint8_t len) {
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

static int vld_checksum(const uint8_t *data, uint8_t len, VehicleState *s) {
    uint8_t expected_cs = can_checksum(data, len);
    uint8_t received_cs = data[len];

    if (expected_cs != received_cs) {
        s->error_code |= ECU_ERR_CAN_CHECKSUM;
        return 0;
    }
    return 1;
}

static int validate_sequence(uint8_t received, CANBus *bus, unsigned index, VehicleState *state) {
    int valid = !bus->sequence_seen[index] || received == (uint8_t)(bus->last_sequence[index] + 1);
    bus->last_sequence[index] = received;
    bus->sequence_seen[index] = 1;
    if (!valid) state->error_code |= ECU_ERR_CAN_SEQ;
    return valid;
}

int can_bus_send(CANBus *bus, const CANFrame *frame) {
    if (bus->count >= CAN_BUS_MAX_FRAMES) return -1;
    bus->frames[bus->count++] = *frame;
    return 0;
}

void can_bus_arbitrate(CANBus *bus) {
     for (int i = 0; i < bus->count - 1; i++) {
        for (int j = i + 1; j < bus->count; j++) {
            if (bus->frames[j].id < bus->frames[i].id) {
                CANFrame tmp = bus->frames[i];
                bus->frames[i] = bus->frames[j];
                bus->frames[j] = tmp;
            }
        }
    }
}

void can_bus_deliver(CANBus *bus, VehicleState *ecu) {
    for (int i = 0; i < bus->count; i++) {
        const CANFrame *frame = &bus->frames[i];
        
        switch (frame->id) {
            case CAN_ID_POWERTRAIN:
                decode_powertrain_frame(frame, ecu, bus);
                break;
            case CAN_ID_DYNAMICS:
                decode_dynamics_frame(frame, ecu, bus);
                break;
            case CAN_ID_SENSORS:
                decode_sensors_frame(frame, ecu, bus);
                break;
            default:
                break;
        }
    }
}

void encode_powertrain_frame(VehicleState *s, CANFrame *f) {
    f->id = CAN_ID_POWERTRAIN;
    f->dlc = CAN_DLC_POWERTRAIN;
    f->data[0] = (s->rpm >> CAN_RPM_SHIFT_MSB) & CAN_BYTE_MASK;
    f->data[1] = s->rpm & CAN_BYTE_MASK;
    f->data[2] = s->gear;
    f->data[3] =
        (s->engine_on << 1) |
        (s->brake << 0);

    // SEQUENCE COUNTER IDX
    f->data[4] = s->transmit_sequence[0]++;

    // CHECKSUM
    f->data[5] = can_checksum(f->data, CAN_DLC_POWERTRAIN-1);
}

void encode_sensors_frame(VehicleState *s, CANFrame *f) {
    f->id = CAN_ID_SENSORS;
    f->dlc = CAN_DLC_SENSORS;

    f->data[0] = s->engine_temp;
    f->data[1] = s->oil_pressure;
    f->data[2] = s->battery_voltage;
    f->data[3] = s->error_code;
    f->data[4] = 0;
    f->data[5] = s->transmit_sequence[2]++;

    // CHECKSUM
    f->data[6] = can_checksum(f->data, CAN_DLC_SENSORS-1);
}

void encode_dynamics_frame(VehicleState *s, CANFrame *f) {
    f->id = CAN_ID_DYNAMICS;
    f->dlc = CAN_DLC_DYNAMICS;

    f->data[0] = s->speed;
    f->data[1] = s->brake_pressure;
    f->data[2] = s->throttle;
    f->data[3] = 0;
    f->data[4] = s->transmit_sequence[1]++;

    // CHECKSUM
    f->data[5] = can_checksum(f->data, CAN_DLC_DYNAMICS-1);
}

int decode_dynamics_frame(const CANFrame *f, VehicleState *s, CANBus *bus) {
    if (f->id != CAN_ID_DYNAMICS || f->dlc != CAN_DLC_DYNAMICS) return -1;
    if (!vld_checksum(f->data, f->dlc-1, s))
        return -1;

    if (!validate_sequence(f->data[4], bus, 1, s))
        return -1;


    s->speed = f->data[0];
    s->brake_pressure = f->data[1];
    s->throttle = f->data[2];

    return 0;
}


int decode_sensors_frame(const CANFrame *f, VehicleState *s, CANBus *bus) {
    if (f->id != CAN_ID_SENSORS || f->dlc != CAN_DLC_SENSORS) return -1;
    if (!vld_checksum(f->data, f->dlc-1, s))
        return -1;

    if (!validate_sequence(f->data[5], bus, 2, s))
        return -1;


    s->engine_temp = f->data[0];
    s->oil_pressure = f->data[1];
    s->battery_voltage = f->data[2];
    s->error_code = (s->error_code & (ECU_ERR_CAN_CHECKSUM | ECU_ERR_CAN_SEQ)) | f->data[3];

    return 0;
}

int decode_powertrain_frame(const CANFrame *f, VehicleState *s, CANBus *bus) {
    if (f->id != CAN_ID_POWERTRAIN || f->dlc != CAN_DLC_POWERTRAIN) return -1;
    if (!vld_checksum(f->data, f->dlc-1, s))
        return -1;

    if (!validate_sequence(f->data[4], bus, 0, s))
        return -1;


    s->rpm = (f->data[0] << 8) | f->data[1];
    s->gear = f->data[2];
    s->engine_on = (f->data[3] & CAN_ENGINE_ON_BIT) != 0;
    s->brake     = (f->data[3] & CAN_BRAKE_BIT) != 0;

    return 0;
}

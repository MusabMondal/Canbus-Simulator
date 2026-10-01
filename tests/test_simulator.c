#include <assert.h>
#include <stdio.h>
#include "can_transport.h"
int main(void) {
    VehicleState tx = {.rpm=4321, .gear=4, .engine_on=1, .speed=43, .throttle=80,
        .engine_temp=90, .oil_pressure=63, .battery_voltage=140};
    VehicleState rx = {0}; CANBus bus = {0}; CANFrame power, dynamics, sensors;
    encode_powertrain_frame(&tx, &power);
    encode_dynamics_frame(&tx, &dynamics);
    encode_sensors_frame(&tx, &sensors);
    assert(can_bus_send(&bus, &sensors)==0);
    assert(can_bus_send(&bus, &power)==0);
    assert(can_bus_send(&bus, &dynamics)==0);
    assert(can_bus_send(&bus, &power)==-1);
    can_bus_arbitrate(&bus);
    assert(bus.frames[0].id==CAN_ID_POWERTRAIN && bus.frames[2].id==CAN_ID_SENSORS);
    can_bus_deliver(&bus, &rx);
    assert(rx.rpm==4321 && rx.speed==43 && rx.engine_temp==90 && !rx.error_code);
    assert(decode_powertrain_frame(&power,&rx,&bus)==-1);
    assert(rx.error_code & ECU_ERR_CAN_SEQ);
    encode_powertrain_frame(&tx,&power);
    power.data[0]^=1;
    assert(decode_powertrain_frame(&power,&rx,&bus)==-1);
    assert(rx.error_code & ECU_ERR_CAN_CHECKSUM);
    power.data[0]^=1;
    assert(decode_powertrain_frame(&power,&rx,&bus)==0);
    encode_powertrain_frame(&tx,&power); /* deliberately lose this message */
    encode_powertrain_frame(&tx,&power);
    assert(decode_powertrain_frame(&power,&rx,&bus)==-1);
    encode_powertrain_frame(&tx,&power);
    assert(decode_powertrain_frame(&power,&rx,&bus)==0);
    for(int i=0;i<300;i++) {
        encode_powertrain_frame(&tx,&power);
        assert(decode_powertrain_frame(&power,&rx,&bus)==0);
    }
    power.dlc=0;
    assert(decode_powertrain_frame(&power,&rx,&bus)==-1);
    VehicleState off = {.rpm=3,.speed=1,.throttle=2,.brake_pressure=3};
    vehicle_update(&off);
    assert(!off.rpm && !off.speed && !off.throttle && !off.brake_pressure);
    VehicleState hot = {.engine_on=1,.engine_temp=120};
    vehicle_update_diagnostics(&hot);
    assert(hot.error_code & ECU_ERR_OVER_TEMP);
    puts("Simulator unit checks passed");
}

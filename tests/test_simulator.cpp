#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "can_transport.h"
}

using testing::ElementsAre;
using testing::Field;

class SimulatorTest : public testing::Test {
protected:
    VehicleState tx{};
    VehicleState rx{};
    CANBus bus{};
    CANFrame power{}, dynamics{}, sensors{};

    void SetUp() override {
        tx.rpm = 4321;
        tx.gear = 4;
        tx.engine_on = 1;
        tx.speed = 43;
        tx.throttle = 80;
        tx.engine_temp = 90;
        tx.oil_pressure = 63;
        tx.battery_voltage = 140;
        encode_powertrain_frame(&tx, &power);
        encode_dynamics_frame(&tx, &dynamics);
        encode_sensors_frame(&tx, &sensors);
    }
};

TEST_F(SimulatorTest, EncodesPowertrainPayload) {
    EXPECT_EQ(power.id, CAN_ID_POWERTRAIN);
    ASSERT_EQ(power.dlc, CAN_DLC_POWERTRAIN);
    EXPECT_THAT(power.data, ElementsAre(0x10, 0xE1, 4, 2, 0, 0xF7, 0, 0));
}

TEST_F(SimulatorTest, ArbitratesFramesByIdentifier) {
    ASSERT_EQ(can_bus_send(&bus, &sensors), 0);
    ASSERT_EQ(can_bus_send(&bus, &power), 0);
    ASSERT_EQ(can_bus_send(&bus, &dynamics), 0);
    can_bus_arbitrate(&bus);
    EXPECT_THAT(bus.frames, ElementsAre(
        Field(&CANFrame::id, CAN_ID_POWERTRAIN),
        Field(&CANFrame::id, CAN_ID_DYNAMICS),
        Field(&CANFrame::id, CAN_ID_SENSORS)));
}

TEST_F(SimulatorTest, RejectsQueueOverflow) {
    for (int i = 0; i < CAN_BUS_MAX_FRAMES; ++i)
        ASSERT_EQ(can_bus_send(&bus, &power), 0);
    EXPECT_EQ(can_bus_send(&bus, &sensors), -1);
    EXPECT_EQ(bus.count, CAN_BUS_MAX_FRAMES);
    EXPECT_THAT(bus.frames, testing::Each(Field(&CANFrame::id, CAN_ID_POWERTRAIN)));
}

TEST_F(SimulatorTest, DeliversAllVehicleFields) {
    ASSERT_EQ(can_bus_send(&bus, &sensors), 0);
    ASSERT_EQ(can_bus_send(&bus, &power), 0);
    ASSERT_EQ(can_bus_send(&bus, &dynamics), 0);
    can_bus_arbitrate(&bus);
    can_bus_deliver(&bus, &rx);
    EXPECT_EQ(rx.rpm, tx.rpm);
    EXPECT_EQ(rx.gear, tx.gear);
    EXPECT_EQ(rx.engine_on, tx.engine_on);
    EXPECT_EQ(rx.brake, tx.brake);
    EXPECT_EQ(rx.speed, tx.speed);
    EXPECT_EQ(rx.brake_pressure, tx.brake_pressure);
    EXPECT_EQ(rx.throttle, tx.throttle);
    EXPECT_EQ(rx.engine_temp, tx.engine_temp);
    EXPECT_EQ(rx.oil_pressure, tx.oil_pressure);
    EXPECT_EQ(rx.battery_voltage, tx.battery_voltage);
    EXPECT_EQ(rx.error_code, ECU_ERR_NONE);
}

TEST_F(SimulatorTest, RejectsDuplicateSequence) {
    ASSERT_EQ(decode_powertrain_frame(&power, &rx, &bus), 0);
    EXPECT_EQ(decode_powertrain_frame(&power, &rx, &bus), -1);
    EXPECT_NE(rx.error_code & ECU_ERR_CAN_SEQ, 0);
}

TEST_F(SimulatorTest, RejectsChecksumWithoutAdvancingSequence) {
    power.data[0] ^= 1;
    EXPECT_EQ(decode_powertrain_frame(&power, &rx, &bus), -1);
    EXPECT_NE(rx.error_code & ECU_ERR_CAN_CHECKSUM, 0);
    EXPECT_EQ(rx.rpm, 0);
    EXPECT_EQ(bus.sequence_seen[0], 0);
    power.data[0] ^= 1;
    EXPECT_EQ(decode_powertrain_frame(&power, &rx, &bus), 0);
}

TEST_F(SimulatorTest, RecoversAfterMissingMessage) {
    ASSERT_EQ(decode_powertrain_frame(&power, &rx, &bus), 0);
    encode_powertrain_frame(&tx, &power); // Lose this message.
    tx.rpm = 5000;
    encode_powertrain_frame(&tx, &power);
    EXPECT_EQ(decode_powertrain_frame(&power, &rx, &bus), -1);
    EXPECT_NE(rx.error_code & ECU_ERR_CAN_SEQ, 0);
    EXPECT_EQ(rx.rpm, 4321);
    encode_powertrain_frame(&tx, &power);
    EXPECT_EQ(decode_powertrain_frame(&power, &rx, &bus), 0);
    EXPECT_EQ(rx.rpm, 5000);
}

TEST_F(SimulatorTest, AcceptsSequenceRollover) {
    ASSERT_EQ(decode_powertrain_frame(&power, &rx, &bus), 0);
    for (int i = 0; i < 300; ++i) {
        SCOPED_TRACE(i);
        encode_powertrain_frame(&tx, &power);
        ASSERT_EQ(decode_powertrain_frame(&power, &rx, &bus), 0);
    }
    EXPECT_EQ(rx.error_code, ECU_ERR_NONE);
}

TEST_F(SimulatorTest, RejectsMalformedPayloadLength) {
    power.dlc = 0;
    EXPECT_EQ(decode_powertrain_frame(&power, &rx, &bus), -1);
    EXPECT_EQ(rx.rpm, 0);
}

TEST(VehicleModelTest, EngineOffValuesReachZeroWithoutUnderflow) {
    VehicleState state{};
    state.rpm = 3;
    state.speed = 1;
    state.throttle = 2;
    state.brake_pressure = 3;
    vehicle_update(&state);
    EXPECT_EQ(state.rpm, 0);
    EXPECT_EQ(state.speed, 0);
    EXPECT_EQ(state.throttle, 0);
    EXPECT_EQ(state.brake_pressure, 0);
}

TEST(VehicleModelTest, ReportsHighEngineTemperature) {
    VehicleState state{};
    state.engine_on = 1;
    state.engine_temp = 120;
    vehicle_update_diagnostics(&state);
    EXPECT_NE(state.error_code & ECU_ERR_OVER_TEMP, 0);
}

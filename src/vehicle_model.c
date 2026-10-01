#include <stdio.h>

#include "vehicle_model.h"
#include "can_transport.h"

void vehicle_update(VehicleState *s) {

    
    if (s->engine_on) {
        if (!s->brake && !s->in_brake_curve) {
            // Normal acceleration
            if (s->throttle < 100) {
                s->throttle = s->throttle > 97 ? 100 : s->throttle + 3;
            }
        } else {
            if (s->throttle > 0) {
                s->throttle = s->throttle < 5 ? 0 : s->throttle - 5;
                if (s->throttle > 100) s->throttle = 100;
            }
        }
        
        if (s->throttle > 0) {
            if (s->rpm < 7000) {
                s->rpm += s->throttle * 2;
            }
        } else {
            // Engine braking
            if (s->rpm > 800) {
                s->rpm = s->rpm < 100 ? 0 : s->rpm - 100;
                if (s->rpm < 700) s->rpm = 700;
            }
        }
        
        if (s->rpm > 7000) {
            s->rpm = 7000;
        }
        
        uint16_t target_speed = s->rpm / 100;
        
        if (s->brake) {
            s->in_brake_curve = 1;
            s->brake_hold_counter = BRAKE_RELEASE_DELAY;
            
            if (s->brake_pressure < 100) {
                s->brake_pressure += 20;
                if (s->brake_pressure > 100) s->brake_pressure = 100;
            }
            
            if (s->speed > MIN_CURVE_SPEED) {
                uint8_t decel = (s->brake_pressure * s->speed) / 200;
                
                if (decel < 2) decel = 2;
                
                if (s->speed > decel + MIN_CURVE_SPEED) {
                    s->speed -= decel;
                } else {
                    s->speed = MIN_CURVE_SPEED;
                }
            }
            
        } else if (s->in_brake_curve) {
            if (s->brake_hold_counter > 0) {
                s->brake_hold_counter--;
                
                if (s->speed < MIN_CURVE_SPEED) {
                    s->speed = MIN_CURVE_SPEED;
                }
                
                if (s->brake_pressure > 0) {
                    s->brake_pressure = s->brake_pressure < 10 ? 0 : s->brake_pressure - 10;
                    if (s->brake_pressure > 100)
                        s->brake_pressure = 100;
                }
                
            } else {
                s->in_brake_curve = 0;
                
                if (s->speed < target_speed) {
                    s->speed += 2;
                }
            }
            
        } else {
            if (s->brake_pressure > 0) {
                s->brake_pressure = s->brake_pressure < 25 ? 0 : s->brake_pressure - 25;
            }
            
            if (s->speed < target_speed) {
                s->speed += 1;
            } else if (s->speed > target_speed) {
                s->speed -= 1; 
            }
        }
        
        if (s->speed > 250) s->speed = 250;
        
        if (s->rpm < 800)
            s->gear = 1;
        else if (s->rpm < 2500)
            s->gear = 2;
        else if (s->rpm < 4000)
            s->gear = 3;
        else if (s->rpm < 5500)
            s->gear = 4;
        else
            s->gear = 5;
            
    } else {
        if (s->rpm > 0) s->rpm = s->rpm < 100 ? 0 : s->rpm - 100;
        if (s->speed > 0) s->speed = s->speed < 2 ? 0 : s->speed - 2;
        if (s->throttle > 0) s->throttle = s->throttle < 5 ? 0 : s->throttle - 5;
        if (s->brake_pressure > 0) s->brake_pressure = s->brake_pressure < 25 ? 0 : s->brake_pressure - 25;
        s->in_brake_curve = 0;
    }
}

void vehicle_update_diagnostics(VehicleState *s) {
    // Engine temperature
    if (s->engine_on) {
        if (s->rpm > 5000 && s->engine_temp < 110)
            s->engine_temp += 2;
        else if (s->engine_temp < 90)
            s->engine_temp += 1; 
    } else {
        if (s->engine_temp > 20)
            s->engine_temp -= 1; 
    }
    
    // Oil pressure
    if (s->engine_on) {
        s->oil_pressure = 20 + (s->rpm / 100);
        if (s->oil_pressure > 90)
            s->oil_pressure = 90;
    } else {
        s->oil_pressure = 0;
    }
    
    // Battery voltage
    if (s->engine_on) {
        s->battery_voltage = 140; 
    } else {
        if (s->battery_voltage > 124)
            s->battery_voltage -= 1;
        else
            s->battery_voltage = 124; 
    }
    
    
    s->error_code = ECU_ERR_NONE;
    if (s->engine_temp > 115) s->error_code |= ECU_ERR_OVER_TEMP;
    if (s->rpm > 7000) s->error_code |= ECU_ERR_OVER_RPM;
    if (s->oil_pressure < 15 && s->rpm > 1000) s->error_code |= ECU_ERR_LOW_OIL;
    if (s->battery_voltage < 110) s->error_code |= ECU_ERR_LOW_BATTERY;

}

void vehicle_print(const VehicleState *s) {
    printf("=== ACTUAL STATE ==\n");
    printf("RPM: %d\n", s->rpm);
    printf("SPEED: %d\n", s->speed);
    printf("THROTTLE: %d\n", s->throttle);
    printf("BRAKE: %d\n", s->brake);
    printf("GEAR: %d\n", s->gear);
    printf("ENGINE TEMP: %d\n", s->engine_temp);
    printf("OIL PRESSURE: %d\n", s->oil_pressure);
    printf("BATTERY_VOLTAGE: %d\n", s->battery_voltage);
    
    if (s->error_code) {
        printf("ERROR CODE: 0x%02X\n", s->error_code);
        if (s->error_code & ECU_ERR_CAN_CHECKSUM) puts("CAN checksum error");
        if (s->error_code & ECU_ERR_CAN_SEQ) puts("CAN sequence error");
        if (s->error_code & ECU_ERR_OVER_TEMP) puts("Engine over temperature");
        if (s->error_code & ECU_ERR_LOW_OIL) puts("Low oil pressure");
        if (s->error_code & ECU_ERR_LOW_BATTERY) puts("Low battery voltage");

    }

    printf("===================\n");
}

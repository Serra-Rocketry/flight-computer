/**
 * @file parachute_module.h
 * @brief Parachute servo actuator module
 *
 * This module owns the parachute release servo and exposes the
 * initialization routine (setupServo). The deployment DECISION is no longer
 * made here — it lives in the FlightStateMachine (apogee detection, Option A)
 * and is acted upon by FlightControlTask, which calls ParachuteServo.write()
 * the moment the FSM confirms deploy conditions.
 *
 * This separation keeps the actuator (this module) decoupled from the
 * detection logic (FSM) and the safety-critical task (FlightControlTask).
 *
 * @author #11
 * @date 2026
 */

#ifndef PARACHUTE_MODULE_H
#define PARACHUTE_MODULE_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

//==============================================================================
// PARACHUTE SERVO (actuator owned by this module)
//==============================================================================

/**
 * Servo motor object for parachute deployment.
 * FlightControlTask actuates it via ParachuteServo.write(SERVO_OPEN)
 * when FlightStateMachine::isParachuteDeployed() becomes true.
 * 
 * Defined in parachute_module.cpp
 */
extern Servo ParachuteServo;

//==============================================================================
// INITIALIZATION
//==============================================================================

/**
 * Initialize servo motor and set to closed (locked) position.
 *
 * Attaches the servo to SERVO_PIN and moves it to SERVO_CLOSED so the
 * parachute compartment is locked before flight operations begin.
 *
 * @param keepOpen If true, leaves the servo at SERVO_OPEN instead. Used after
 *        a watchdog reboot when the FSM snapshot says the parachute was
 *        already deployed mid-flight — closing the compartment on a deployed
 *        chute would release it at altitude.
 *
 * @note Called from initFlightControlTask() during setup.
 * @see config.h for SERVO_PIN, SERVO_CLOSED, and SERVO_OPEN.
 * @return true if the servo attached successfully, false otherwise.
 */
bool setupServo(bool keepOpen = false);

#endif // PARACHUTE_MODULE_H

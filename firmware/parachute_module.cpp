/**
 * @file parachute_module.cpp
 * @brief Parachute servo actuator module implementation
 * 
 * Defines the global Servo object used by FlightControlTask.
 * 
 * @author #11
 * @date 2026
 */

#include "modules/parachute_module.h"
#include "config.h"

//==============================================================================
// GLOBAL DEFINITIONS
//==============================================================================

/**
 * Servo motor object for parachute deployment.
 * FlightControlTask actuates it via ParachuteServo.write(SERVO_OPEN)
 * when FlightStateMachine::isParachuteDeployed() becomes true.
 */
Servo ParachuteServo;

//==============================================================================
// IMPLEMENTATION
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
bool setupServo(bool keepOpen) {
  // ESP32Servo::attach() returns true on success.
  if (!ParachuteServo.attach(SERVO_PIN)) {
    return false;
  }

  // Move to closed/locked position — unless the chute was already deployed
  // before a mid-flight reboot.
  ParachuteServo.write(keepOpen ? SERVO_OPEN : SERVO_CLOSED);

  return true;
}
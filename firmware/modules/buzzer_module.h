/**
 * @file buzzer_module.h
 * @brief Audio feedback and status indication system using piezo buzzer
 * 
 * This module provides audio signaling capabilities for the flight computer,
 * generating different sound patterns to indicate system states and events.
 * The buzzer provides critical feedback during operation, especially when
 * visual monitoring is not possible.
 * 
 * @author Team #100 Avionics
 * @date 2024
 */

#ifndef BUZZER_MODULE_H
#define BUZZER_MODULE_H

#include <Arduino.h>
#include "config.h"

//==============================================================================
// AUDIO SIGNAL GENERATION
//==============================================================================

/**
 * @brief Generate audio signals using the buzzer for status indication
 * 
 * Produces different sound patterns based on the signal type to provide
 * audible feedback about system state and events. Each pattern uses a
 * specific sequence of tones to be easily distinguishable.
 * 
 * Signal patterns:
 * - "Alert": 5 beeps (200ms each, 350ms interval) - indicates errors during initialization
 * - "Success": 3 beeps (100ms each, 200ms interval) - indicates successful initialization
 * - "Activated": Single long beep (500ms) - indicates parachute deployment
 * - "Beep": Short beep (50ms) - standard operation confirmation
 * 
 * @param signal Signal type string: "Alert", "Success", "Activated", or "Beep"
 * 
 * @note All signals use 500Hz frequency for consistency
 * @note The function blocks during tone generation due to delay() calls
 * @warning Invalid signal types will print an error message to Serial
 * 
 * @see buzzSignal() is called by telemetry_module.h for operation feedback
 * @see BUZZER_PIN is defined in config.h
 */
void buzzSignal(String signal)
{
  int frequency = 500;   // Tone frequency in Hz (500Hz chosen for audibility)
  
  if (signal == "Alert") // Error signal during initialization
  {
    // Generate 5 rapid beeps to indicate critical error
    for (int i = 0; i < 5; i++)
    {
      tone(BUZZER_PIN, frequency, 200);  // 200ms beep
      delay(200 + 150);                   // Wait for beep + 150ms pause
    }
  }
  else if (signal == "Success") // Success signal on initialization
  {
    // Generate 3 medium beeps to indicate successful startup
    for (int i = 0; i < 3; i++)
    {
      tone(BUZZER_PIN, frequency, 100);  // 100ms beep
      delay(100 + 100);                   // Wait for beep + 100ms pause
    }
  }
  else if (signal == "Activated") // Parachute deployment confirmation
  {
    // Single long beep to indicate parachute has been deployed
    tone(BUZZER_PIN, frequency, 500);    // 500ms sustained tone
  }
  else if (signal == "Beep") // Standard operation beep
  {
    // Short beep for routine operation feedback (e.g., data transmission)
    tone(BUZZER_PIN, frequency, 50);     // 50ms quick beep
    delay(100);                           // Small delay to prevent overlap
  }
  else
  {
    // Invalid signal type - log error for debugging
    Serial.println("Invalid signal!");
  }
}

#endif // BUZZER_MODULE_H

/**
 * @file lora_module.h
 * @brief LoRa wireless communication module
 * 
 * This module manages long-range wireless communication using LoRa technology.
 * LoRa (Long Range) provides:
 * - Long-distance communication (several kilometers)
 * - Low power consumption
 * - Good penetration through obstacles
 * - Resilient to interference
 * 
 * Used for:
 * - Real-time telemetry transmission during flight
 * - Ground station communication
 * - Remote monitoring and tracking
 * 
 * Communication: SPI bus
 * Frequency: 868 MHz (Europe) or 915 MHz (Americas) - configured in config.h
 * 
 * @author Team #100
 * @date 2026
 */

#ifndef LORA_MODULE_H
#define LORA_MODULE_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

//==============================================================================
// INITIALIZATION FUNCTIONS
//==============================================================================

/**
 * Initialize LoRa communication module
 * 
 * Configures the LoRa module with pins and operating parameters.
 * Sets up SPI communication and initializes the radio at specified frequency.
 * 
 * Configuration:
 * - Frequency: Defined in config.h (LORA_FREQ)
 * - Sync word: 0xF3 (ensures only matching devices communicate)
 * - SPI pins: SS, RST, DIO0 defined in config.h
 * 
 * @return true if initialization was successful
 * @return false if module not found or communication failed
 * 
 * @note Ensure correct frequency for your region (868MHz EU, 915MHz Americas)
 * @warning Operating on wrong frequency may violate local regulations
 * @see config.h for pin and frequency configuration
 */
bool setupLoRa()
{
  // Configure SPI pins for LoRa module
  LoRa.setPins(SS_LORA, RST_LORA, DIO0_LORA);
  
  // Initialize LoRa at configured frequency
  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println("LoRa initialization failed.");
    return false;
  }
  
  // Set synchronization word (network ID)
  // Only devices with same sync word can communicate
  LoRa.setSyncWord(SYNC_WORD);
  
  return true;
}

//==============================================================================
// TRANSMISSION FUNCTIONS
//==============================================================================

/**
 * Send message via LoRa wireless communication
 * 
 * Transmits a text message over LoRa radio. The function:
 * 1. Begins a new packet
 * 2. Writes message content
 * 3. Ends packet and transmits
 * 4. Reports transmission status
 * 
 * @param message String message to transmit via LoRa
 * 
 * @note Maximum packet size depends on LoRa configuration (typically 255 bytes)
 * @note Transmission time increases with message length
 * @note Function blocks until transmission completes
 * @see printBoth() in telemetry_module.h for combined Serial+LoRa output
 */
void sendLoRa(const String &message)
{
  // Start a new LoRa packet
  LoRa.beginPacket();
  
  // Write message to packet buffer
  LoRa.print(message);
  
  // Finalize and transmit packet
  if (LoRa.endPacket())
  {
    // Transmission successful
    Serial.println("LoRa message sent.");
  }
  else
  {
    // Transmission failed (unlikely if initialization succeeded)
    Serial.println("ERROR sending LoRa message!");
  }
}

#endif // LORA_MODULE_H

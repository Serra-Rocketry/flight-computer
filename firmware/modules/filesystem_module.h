/**
 * @file filesystem_module.h
 * @brief LittleFS filesystem management module
 * 
 * This module manages the internal flash filesystem (LittleFS) for data logging.
 * LittleFS provides:
 * - Persistent data storage
 * - Power-fail resilient file operations
 * - Wear leveling for flash memory
 * - Small memory footprint
 * 
 * Used for:
 * - Storing telemetry data in CSV format
 * - Flight data backup (independent of wireless transmission)
 * - Web server file serving
 * - Post-flight data analysis
 * 
 * @author Team #100
 * @date 2026
 */

#ifndef FILESYSTEM_MODULE_H
#define FILESYSTEM_MODULE_H

#include <Arduino.h>
#include "LittleFS.h"

//==============================================================================
// INITIALIZATION FUNCTIONS
//==============================================================================

/**
 * Initialize LittleFS filesystem
 * 
 * Mounts the internal flash filesystem and prepares it for use.
 * If filesystem is not formatted, it will be formatted automatically.
 * 
 * @return true if filesystem mounted successfully
 * @return false if mount/format failed
 * 
 * @note First boot may take longer due to formatting
 * @warning If returns false, data logging will fail
 * @see writeFile() and appendFile() require successful initialization
 */
bool setupLittleFS()
{
  // Mount LittleFS with auto-format on failure
  if (!LittleFS.begin(true))
  {
    Serial.println("Error mounting LittleFS.");
    return false;
  }
  return true;
}

//==============================================================================
// FILE WRITE FUNCTIONS
//==============================================================================

/**
 * Write data to file (creates new file or overwrites existing)
 * 
 * Opens file in write mode, writes data, and closes file.
 * If file exists, it will be overwritten. Use appendFile() to add to existing files.
 * 
 * Typical use: Creating new CSV file with header row
 * 
 * @param path File path (e.g., "/data.csv")
 * @param data_string Data string to write to file
 * @return true if write operation successful
 * @return false if file couldn't be opened or write failed
 * 
 * @note Path must start with "/" (root directory)
 * @warning This function OVERWRITES existing files
 * @see appendFile() for adding data to existing files
 */
bool writeFile(const String &path, const String &data_string)
{
  // Open file in write mode (creates new or overwrites existing)
  File file = LittleFS.open(path, FILE_WRITE);
  
  if (!file)
  {
    Serial.println("Failed to open file for writing.");
    return false;
  }
  
  // Write data with newline
  if (file.println(data_string))
  {
    Serial.println("File written.");
  }
  else
  {
    Serial.println("File write failed.");
    file.close();
    return false;
  }
  
  file.close();
  return true;
}

//==============================================================================
// FILE APPEND FUNCTIONS
//==============================================================================

/**
 * Append data to existing file
 * 
 * Opens file in append mode and adds new line of data.
 * Creates file if it doesn't exist. Data is added at end without overwriting.
 * 
 * Typical use: Adding telemetry data rows to CSV file
 * 
 * @param path File path (e.g., "/data.csv")
 * @param message Message/data to append to file
 * 
 * @note Each call adds a new line (println is used)
 * @note File is automatically closed after append
 * @see writeFile() for creating new files with headers
 */
void appendFile(const String &path, const String &message)
{
  // Open file in append mode
  File file = LittleFS.open(path, FILE_APPEND);
  
  if (!file)
  {
    Serial.println("Failed to open file for appending.");
    return;
  }

  // Append message with newline
  if (!file.println(message))
  {
    Serial.println("Failed to append message.");
  }
  
  file.close();
}

#endif // FILESYSTEM_MODULE_H

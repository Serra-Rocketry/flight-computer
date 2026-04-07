/**
 * @file server_module.h
 * @brief WiFi access point and web server for flight data management
 * 
 * This module creates a WiFi access point and serves a web interface for
 * accessing, viewing, and managing flight data stored in the LittleFS filesystem.
 * The web server provides both static file serving and RESTful API endpoints
 * for file operations.
 * 
 * Features:
 * - WiFi access point creation for wireless connectivity
 * - Web interface for browsing flight data files
 * - REST API for listing, downloading, and deleting files
 * - Asynchronous request handling for better performance
 * 
 * @note The server runs on port 80 (standard HTTP port)
 * @note WiFi credentials (SSID and password) are defined in config.h
 * 
 * @author Team #100 Avionics
 * @date 2024
 */

#ifndef SERVER_MODULE_H
#define SERVER_MODULE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "LittleFS.h"
#include "config.h"

//==============================================================================
// GLOBAL VARIABLES
//==============================================================================

/**
 * @brief Asynchronous HTTP server instance listening on port 80
 * 
 * Uses ESPAsyncWebServer library for non-blocking request handling,
 * allowing the main loop to continue processing sensor data while
 * serving web requests concurrently.
 */
AsyncWebServer server(80);

//==============================================================================
// WEB SERVER ROUTE CONFIGURATION
//==============================================================================

/**
 * @brief Configure all web server routes for static files and API endpoints
 * 
 * Sets up the complete routing table for the web server, including:
 * 1. Static file routes for the web interface (HTML, CSS, JavaScript)
 * 2. API routes for file management operations
 * 
 * API Endpoints:
 * - GET /api/files - Returns JSON array of all files with metadata
 * - GET /api/file?filename=X - Returns specific file content or downloads it
 * - DELETE /api/file?filename=X - Deletes specified file from filesystem
 * 
 * @note All routes use lambda functions for inline request handling
 * @note API responses use UTF-8 encoding for proper character support
 * @warning File operations are performed directly on LittleFS without undo capability
 * 
 * @see setupServer() calls this function during initialization
 */
void setServerRoutes()
{
  //----------------------------------------------------------------------------
  // STATIC FILE ROUTES - Web Interface
  //----------------------------------------------------------------------------
  
  // Serve main HTML page at root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/server/index.html", "text/html"); });

  // Serve CSS stylesheet for web interface styling
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/server/style.css", "text/css"); });

  // Serve JavaScript file for web interface functionality
  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/server/index.js", "application/javascript"); });

  //----------------------------------------------------------------------------
  // API ROUTES - File Management Endpoints
  //----------------------------------------------------------------------------
  // Note: Not strictly RESTful due to library limitations and ESP32 constraints
  
  /**
   * GET /api/files - List all files in filesystem root with metadata
   * 
   * Returns a JSON array containing all files in the root directory.
   * Each file object includes:
   * - name: filename string
   * - size: file size in bytes
   * 
   * Response format: [{"name": "file1.csv", "size": 1234}, ...]
   * Used by web interface to populate the file list table.
   */
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument fsFiles; // Create JSON document for file list
    File root = LittleFS.open("/"); // Open filesystem root directory

    // Iterate through all files in root directory
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        // Add file metadata to JSON array
        JsonObject fsFile = fsFiles.add<JsonObject>();
        fsFile["name"] = String(file.name());
        fsFile["size"] = file.size();
      }
      file = root.openNextFile();
    }
    
    // Serialize JSON and send response to client
    char jsonString[8000] = { 0 };  // Buffer for JSON string (supports ~100-200 files)
    serializeJson(fsFiles, jsonString);
    request->send(200, "application/json; charset=utf-8", jsonString); });

  /**
   * GET /api/file?filename=X - Retrieve specific file from filesystem
   * 
   * Query parameters:
   * - filename (required): Name of file to retrieve
   * - download (optional): If present, triggers browser download instead of display
   * 
   * Returns:
   * - 200: File content (text/plain or download)
   * - 400: Missing filename parameter
   * - 404: File not found
   * 
   * Used by web interface for file viewing and downloading.
   */
  server.on("/api/file", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    // Validate required filename parameter
    if (!(request->hasParam("filename"))) {
      request->send(400, "text/plain; charset=utf-8", "Missing URL parameter <filename>.");
      return;
    }

    // Check if requested file exists in filesystem
    const AsyncWebParameter* param = request->getParam("filename");
    String filename = param->value();
    if (!LittleFS.exists("/" + filename)) {
      request->send(404, "text/plain; charset=utf-8", "File <" + filename + 
          "> not found in filesystem");
      return;
    }

    // Send file as download or plain text based on download parameter
    if (request->hasParam("download")) {
      // Trigger browser download dialog
      request->send(LittleFS, "/" + filename, String(), true);
    } else {
      // Display file content in browser as plain text
      request->send(LittleFS, "/" + filename, "text/plain; charset=utf-8");
    } });

  /**
   * DELETE /api/file?filename=X - Delete specific file from filesystem
   * 
   * Query parameters:
   * - filename (required): Name of file to delete
   * 
   * Returns:
   * - 200: File deleted successfully
   * - 400: Missing filename parameter or deletion failed
   * - 404: File doesn't exist
   * 
   * @warning This operation is irreversible - deleted files cannot be recovered
   * 
   * Used by web interface file deletion button.
   */
  server.on("/api/file", HTTP_DELETE, [](AsyncWebServerRequest *request)
            {
    Serial.println(request->method());  // Log request method for debugging
    
    // Validate required filename parameter
    if (!(request->hasParam("filename"))) {
      request->send(400, "text/plain; charset=utf-8", "Missing URL parameter <filename>.");
      return;
    }

    const AsyncWebParameter* param = request->getParam("filename");
    String filename = param->value();

    // Check if file exists before attempting deletion
    if (!LittleFS.exists("/" + filename)) {
      request->send(404, "text/plain; charset=utf-8", "Cannot delete non-existent file <" + filename + ">.");
      return;
    }

    // Attempt to delete file and report result
    if (LittleFS.remove("/" + filename)) {
      request->send(200, "text/plain; charset=utf-8", "File deleted successfully.");
    } else {
      request->send(400, "text/plain; charset=utf-8", "Unknown error deleting file <" + filename + ">.");
    } });
}

//==============================================================================
// SERVER INITIALIZATION
//==============================================================================

/**
 * @brief Initialize WiFi access point and start web server
 * 
 * Creates a WiFi access point using credentials from config.h and starts
 * the asynchronous web server. The access point allows ground station
 * computers to connect wirelessly and access flight data.
 * 
 * Steps performed:
 * 1. Create WiFi access point with configured SSID and password
 * 2. Print access point IP address to Serial (typically 192.168.4.1)
 * 3. Configure all server routes (static files and API endpoints)
 * 4. Start the web server to begin accepting requests
 * 
 * @note SSID and password must be defined in config.h
 * @note Default ESP32 AP IP address is usually 192.168.4.1
 * @note Server runs asynchronously without blocking main loop
 * 
 * @see setServerRoutes() is called internally to configure routing
 * @see ssid and password are defined in config.h
 */
void setupServer()
{
  // Create wireless access point with configured credentials
  WiFi.softAP(ssid, password);
  Serial.println("Creating WiFi access point...");
  Serial.println(WiFi.softAPIP());  // Print IP address for user reference

  // Configure server routes and start accepting connections
  setServerRoutes();
  server.begin();
}

#endif // SERVER_MODULE_H

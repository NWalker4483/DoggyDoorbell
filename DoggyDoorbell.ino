#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>

// Pin definitions
const int buttonPin = 19;  // Button input pin

// Global objects
Preferences preferences;
HTTPClient http;

// State variables
bool buttonState = false;
bool lastButtonState = false;
String triggerURL = "";

// WiFi credentials will be loaded from preferences
String activeSSID = "";
String activePassword = "";

// Menu system
const char* MENU_PROMPT = "\n=== Doggy Doorbell Menu ===\n"
                         "1. Test doorbell trigger\n"
                         "2. Scan and connect to WiFi\n"
                         "3. Configure trigger URL\n"
                         "4. Show current settings\n"
                         "Enter choice (1-4): ";

const char* WELCOME_MSG = "\n*********************************\n"
                         "* Welcome to Doggy Doorbell      *\n"
                         "* Configuration Terminal         *\n"
                         "*********************************\n";

// Wait for serial input with timeout
String readSerialTimeout(unsigned long timeout = 30000) {
    String input = "";
    unsigned long startTime = millis();
    
    while ((millis() - startTime) < timeout) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (input.length() > 0) {
                    return input;
                }
            } else {
                input += c;
            }
        }
        delay(10);
    }
    return input;
}

// Scan for WiFi networks and return selected SSID
String scanAndSelectNetwork() {
    Serial.println("\nScanning WiFi networks...");
    int numNetworks = WiFi.scanNetworks();
    
    if (numNetworks == 0) {
        Serial.println("No networks found!");
        return "";
    }
    
    Serial.printf("\nFound %d networks:\n", numNetworks);
    for (int i = 0; i < numNetworks; ++i) {
        Serial.printf("%d. %s (Signal: %d dBm)\n", 
                    i + 1, 
                    WiFi.SSID(i).c_str(), 
                    WiFi.RSSI(i));
        delay(10);
    }
    
    Serial.println("\nEnter number of network to connect to (1-" + String(numNetworks) + "): ");
    String selection = readSerialTimeout();
    int networkIndex = selection.toInt() - 1;
    
    if (networkIndex >= 0 && networkIndex < numNetworks) {
        return WiFi.SSID(networkIndex);
    }
    
    Serial.println("Invalid selection!");
    return "";
}

// Configure WiFi settings
void configureWiFi() {
    String newSSID = scanAndSelectNetwork();
    if (newSSID.length() == 0) {
        return;
    }
    
    Serial.printf("\nEnter password for %s: ", newSSID.c_str());
    String newPassword = readSerialTimeout();
    if (newPassword.length() == 0) {
        Serial.println("Password entry timed out");
        return;
    }
    
    // Save to preferences
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", newSSID);
    preferences.putString("password", newPassword);
    preferences.end();
    
    Serial.println("\nWiFi credentials saved. Reconnecting...");
    
    // Attempt to connect with new credentials
    WiFi.disconnect();
    WiFi.begin(newSSID.c_str(), newPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected to %s\nIP: %s\n", 
                      newSSID.c_str(), 
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nFailed to connect. Please check credentials.");
    }
}

// Configure trigger URL
void configureTriggerURL() {
    Serial.println("\nEnter trigger URL (e.g., https://trigger.esp8266-server.de/api/?id=...): ");
    String newURL = readSerialTimeout();
    
    if (newURL.length() == 0) {
        Serial.println("URL entry timed out");
        return;
    }
    
    // Basic URL validation
    if (!newURL.startsWith("http://") && !newURL.startsWith("https://")) {
        Serial.println("Invalid URL! Must start with http:// or https://");
        return;
    }
    
    // Save to preferences
    preferences.begin("wifi-config", false);
    preferences.putString("trigger-url", newURL);
    preferences.end();
    
    triggerURL = newURL;
    Serial.println("Trigger URL saved successfully!");
}

// Show current settings
void showSettings() {
    preferences.begin("wifi-config", true);
    
    Serial.println("\nCurrent Settings:");
    Serial.println("----------------");
    Serial.printf("SSID: %s\n", preferences.getString("ssid", "Not set").c_str());
    Serial.printf("Password: %s\n", "********");
    Serial.printf("WiFi Status: %s\n", 
                  WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    }
    Serial.printf("Trigger URL: %s\n", 
                  preferences.getString("trigger-url", "Not set").c_str());
    
    preferences.end();
}

// Process menu choices
void handleMenu() {
    Serial.print(MENU_PROMPT);
    
    String choice = readSerialTimeout();
    
    switch (choice.toInt()) {
        case 1:
            Serial.println("\nTesting doorbell trigger...");
            triggerDoorbell();
            break;
            
        case 2:
            configureWiFi();
            break;
            
        case 3:
            configureTriggerURL();
            break;
            
        case 4:
            showSettings();
            break;
            
        default:
            Serial.println("\nInvalid choice!");
            break;
    }
}

void triggerDoorbell() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected! Cannot trigger.");
        return;
    }
    
    if (triggerURL.length() == 0) {
        Serial.println("Trigger URL not configured!");
        return;
    }
    
    Serial.println("Triggering doorbell...");
    
    http.begin(triggerURL);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode <= 0) {
        Serial.printf("First attempt failed with code: %d\n", httpResponseCode);
        delay(50); // Wait 100ms before retry
        httpResponseCode = http.GET(); // Retry the request
    }
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
        Serial.printf("Response: %s\n", response.c_str());
    } else {
        Serial.printf("Error code: %d\n", httpResponseCode);
    }
    
    http.end();
}

void setup() {
    Serial.begin(115200);
    Serial.println(WELCOME_MSG);
    
    // Configure button pin with internal pulldown
    pinMode(buttonPin, INPUT_PULLDOWN);
    
    // Initialize preferences and load settings
    preferences.begin("wifi-config", true);
    activeSSID = preferences.getString("ssid", "");
    activePassword = preferences.getString("password", "");
    triggerURL = preferences.getString("trigger-url", "");
    preferences.end();
    
    // Connect to WiFi if credentials exist
    if (activeSSID.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(activeSSID.c_str(), activePassword.c_str());
        
        Serial.printf("\nConnecting to %s ", activeSSID.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("Connection failed. Use menu to reconfigure.");
        }
    }
    
    // Display initial menu
    handleMenu();
}

void loop() {
    // Check physical button with pulldown
    buttonState = digitalRead(buttonPin);  // Now reading directly since we use pulldown
    if (buttonState != lastButtonState && buttonState == HIGH) {  // Changed to HIGH since we're using pulldown
        triggerDoorbell();
        delay(100);  // Small debounce delay
    }
    lastButtonState = buttonState;
    
    // Check for serial input
    if (Serial.available()) {
        handleMenu();
    }
}

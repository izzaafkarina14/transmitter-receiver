#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <SoftwareSerial.h>
#include <TimeLib.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "bangg?"
#define WIFI_PASSWORD "hwvv1212"

// Insert Firebase project API Key
#define API_KEY "AIzaSyBnPADOix0EWKu29eysInzDlbA5Rvv9Lz8"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://bismillah-ambil-data-beneran-default-rtdb.asia-southeast1.firebasedatabase.app" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

// Function prototype
String getFormattedTime();

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
int mq4sensor = A0;
int sensorvalue;
String firestatus = "";

SoftwareSerial mySerial(5, 6); // RX, TX for software serial communication
unsigned long sendTimePrevMillis = 0;
unsigned long receiveTimePrevMillis = 0;

void setup() {
  Serial.begin(9600);
  pinMode(mq4sensor, INPUT); // MQ4 sensor
  Serial.println(F("\nESP8266 WiFi scan example"));

  // Set WiFi to station mode
  WiFi.mode(WIFI_STA);

  // Disconnect from an AP if it was previously connected
  WiFi.disconnect();
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
    
  mySerial.begin(9600);

  // Set the time zone for Jakarta, Indonesia
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  }

  // Function to get the current time in the specified format
  String getFormattedTime() {
      // Get the current time in UTC
      time_t now = time(nullptr);

      // Convert the time to Jakarta time zone (WIB, UTC+7)
      now += 7 * 3600;

      // Extract hours, minutes, and seconds
      int hours = hour(now);
      int minutes = minute(now);
      int seconds = second(now);
      int milliseconds = millis() % 1000;

      // Format the time as HH:MM:SS.SSS
      String formattedTime = String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds) + "." + String(milliseconds);
      
      return formattedTime;
}

void loop() {
  int32_t rssi;
  int scanResult;

  Serial.println(F("Scanning WiFi..."));

  scanResult = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);

  if (scanResult > 0) {
    
    // Find the specified WiFi network in the scan results
    for (int8_t i = 0; i < scanResult; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.equals(WIFI_SSID)) {
        rssi = WiFi.RSSI(i); // Get RSSI directly

        Serial.printf(PSTR("WiFi network found: %s\n"), ssid.c_str());
        
        // Display RSSI
        Serial.printf(PSTR("RSSI: %d dBm\n"), rssi);

        // Send RSSI to Firebase
        if (Firebase.ready() && signupOK) {
          Firebase.RTDB.setFloat(&fbdo, "WiFi/RSSI", rssi);
        }

        break; // Stop scanning once the specified network is found
      }
    }
  } else {
    Serial.println(F("No WiFi networks found"));
  }

  // Wait a bit before scanning again
  delay(5000);

  // Transmitter (ESP8266)
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    sensorvalue = analogRead(mq4sensor); // read the MQ4 sensor

    // Get the formatted time without milliseconds
    String currentTime = getFormattedTime();
    
    // Send MQ4 sensor value to Firebase with the current time
    Firebase.RTDB.setFloat(&fbdo, "MQ4 Sensor/Value", sensorvalue);
    Firebase.RTDB.setString(&fbdo, "Transmitter/Time", currentTime);

    // Send the data to another device using software serial
    mySerial.println(currentTime + "," + String(sensorvalue));
  }

  // Receive time information (receiver) from Firebase
  if (Firebase.ready() && signupOK && (millis() - receiveTimePrevMillis > 5000 || receiveTimePrevMillis == 0)) {
      receiveTimePrevMillis = millis();

      // Retrieve the time information (receiver) from Firebase
      if (Firebase.RTDB.getString(&fbdo, "Transmitter/Time")) {
          // Store the received time
          String receivedTime = fbdo.stringData();
          Serial.println("Received Time: " + receivedTime);
          // Process received time information as needed
      } else {
          // If the path doesn't exist, create it and set an initial value
          String initialTime = "00:00:00.000";
          Firebase.RTDB.setString(&fbdo, "Receiver/Time", initialTime);
          Serial.println("Created path 'Receiver/Time' with initial value: " + initialTime);
      }
    }
  else {
      Serial.println("Firebase not ready or signup failed, or not enough time has passed since the last attempt.");    
  }
}
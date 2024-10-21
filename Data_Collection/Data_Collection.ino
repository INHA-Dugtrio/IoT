#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>


#include <HTTPClient.h>
#include <Wire.h>

#include <WiFi.h>
#include "PacketHandler.h"

// Pin definitions
#define RX_PIN 18
#define TX_PIN 17
#define COLLECTION_DELAY 20000  

// WiFi credentials and server configuration
const char* ssid = "YOUR_SSID";  // Change to actual SSID
const char* password = "YOUR_PW";  // Change to actual password
const char* server_name = "YOUR_SERVER";  // Change to actual server address

// Shared resources
hw_timer_t *timer = NULL;
TaskHandle_t dataCollectionTaskHandle = NULL;
TaskHandle_t sendDataTaskHandle = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
StaticJsonDocument<5000> jsonDoc;
SemaphoreHandle_t jsonMutex = xSemaphoreCreateMutex();

// Function declarations
void pushSensorData(JsonArray&, Adafruit_MPU6050&);
void pushPacketData(JsonArray&);
void IRAM_ATTR onTimer();  // Timer interrupt handler

// ============================================================================
// ============================================================================  
// ============================== SETUP FUNCTION ==============================
// ============================================================================
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("After 10s start");
  delay(10000);  // 10-second delay

  // Create the data collection task on core 0
  xTaskCreatePinnedToCore(CollectionDataTask, "CollectionDataTask", 65536, NULL, 3, &dataCollectionTaskHandle, 0);

  // Create the data sending task on core 1
  xTaskCreatePinnedToCore(SendDataTask, "SendDataTask", 65536, NULL, 2, &sendDataTaskHandle, 1);

  // Initialize and set up the timer
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, COLLECTION_DELAY, true, 0);  // Trigger interrupt every 20ms
}

void loop() {
  // No code needed here, tasks handle everything
  vTaskDelay(portMAX_DELAY);
}


// ============================================================================
// ============================================================================
// =========================== DATA COLLECTION TASK ===========================
// ============================================================================
// ============================================================================
void CollectionDataTask(void* p) {
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);  // Initialize Serial2

  Adafruit_MPU6050 mpu;  // Create MPU6050 object
  unsigned long timestamp = 0, counter = 0;

  // Check if MPU6050 is initialized
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (true) delay(1000);
  }
  Serial.println("MPU6050 Found!");

  // Configure MPU6050 settings
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Data collection loop
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Wait for timer notification

    if (xSemaphoreTake(jsonMutex, portMAX_DELAY)) {
      // Create new JSON entry
      JsonArray entry = jsonDoc.createNestedArray();
      entry.add(timestamp);

      // Add sensor and packet data
      pushSensorData(entry, mpu);
      pushPacketData(entry);

      xSemaphoreGive(jsonMutex);  // Release the mutex
    }

    counter++;
    timestamp += COLLECTION_DELAY / 1000;  // Update timestamp


    if (counter >= (1000000 / COLLECTION_DELAY)) {
      counter = 0;
      xTaskNotifyGive(sendDataTaskHandle);  // Notify SendDataTask
    }
  }
}


// ============================================================================
// ============================================================================
// ============================ DATA SENDING TASK ============================
// ============================================================================
// ============================================================================
void SendDataTask(void *p) {
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Wait for data collection notification

    // Reconnect if WiFi is disconnected
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);

      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
      }
      Serial.println("\nReconnected to WiFi!");
    }

    WiFiClient client;
    HTTPClient http;

    // Start HTTP connection
    http.begin(client, server_name);
    http.addHeader("Content-Type", "application/json");

    // Serialize and send JSON data
    String jsonString;
    if (xSemaphoreTake(jsonMutex, portMAX_DELAY)) {
      serializeJson(jsonDoc, jsonString);  // Convert JSON to string
      jsonDoc.clear();  // Clear JSON after sending
      xSemaphoreGive(jsonMutex);
    }

    // Send HTTP POST request
    Serial.print("Sending JSON Data: ");
    Serial.println(jsonString);
    int httpResponseCode = http.POST(jsonString);

    // Handle server response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response Code: " + String(httpResponseCode));
      Serial.println("Response: " + response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();  // End HTTP connection
    Serial.println("Data sent and JSON buffer cleared.");
  }
}


// ============================================================================
// ============================================================================
// ============================== TIMER INTERRUPT ==============================
// ============================================================================
// ============================================================================
void IRAM_ATTR onTimer() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // Notify the data collection task
  vTaskNotifyGiveFromISR(dataCollectionTaskHandle, &xHigherPriorityTaskWoken);

  // Yield to higher-priority task if needed
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}


// ============================================================================
// ============================================================================
// ============================ PUSH SENSOR DATA ============================
// ============================================================================
// ============================================================================
void pushSensorData(JsonArray& entry, Adafruit_MPU6050& mpu) {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);  // Get sensor data

  // Add acceleration data to JSON
  entry.add(a.acceleration.x);
  entry.add(a.acceleration.y);
  entry.add(a.acceleration.z);
}


// ============================================================================
// ============================================================================
// ============================ PUSH PACKET DATA ============================
// ============================================================================
// ============================================================================
void pushPacketData(JsonArray& entry) {

  Frame* dataFrame;  // Pointer to hold the packet data
  while(true) {
    // Wait for ACK signal from the PacketHandler
    if(PacketHandler::readByteWithBlocking() == ACK) {
      dataFrame = PacketHandler::readPacket();  // Attempt to read the packet

      // Check if dataFrame is valid (non-nullptr) before proceeding
      if(dataFrame != nullptr) {
        break;  // Exit the loop if a valid packet is received
      }
      // If dataFrame is nullptr, loop continues to wait for a valid packet
    }
  }

  // Bug fix: The previous version assumed dataFrame was always valid,
  // but it could be nullptr, leading to potential crashes.

  // Add packet data to the JSON array
  for (int i = 0; i < dataFrame->size; i++) {
    entry.add(dataFrame->data[i]);  // Append packet data to the JSON entry
  }

  delete dataFrame;  // Free the memory allocated for the packet
}


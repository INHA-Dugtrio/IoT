#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#include <WiFi.h>
#include <ArduinoJson.h>
#include "PacketHandler.h"

// Pin definitions
#define RX_PIN 18
#define TX_PIN 17
#define COLLECTION_DELAY 50000

// WiFi credentials
const char* ssid = "";      // Replace with your WiFi SSID
const char* password = "";  // Replace with your WiFi password

// TCP server configuration
const char* serverIP = "";  // Replace with your server IP
const int serverPort = 3000;             // Replace with your server port

// Shared resources
QueueHandle_t dataQueue;
WiFiClient tcpClient;

// Timer
hw_timer_t* timer = NULL;
TaskHandle_t dataCollectionTaskHandle = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
void IRAM_ATTR onTimer();  // Timer interrupt handler

// Function declarations
void pushSensorData(JsonArray&, Adafruit_MPU6050&);
void pushPacketData(JsonArray&);
void CollectionDataTask(void* p);
void SendDataTask(void* p);

void setup() {
  Serial.begin(115200);
  Serial.println("After 10s start");
  delay(10000);

  // Create tasks
  xTaskCreatePinnedToCore(CollectionDataTask, "CollectionDataTask", 65536, NULL, 3, &dataCollectionTaskHandle, 0);
  xTaskCreatePinnedToCore(SendDataTask, "SendDataTask", 65536, NULL, 2, NULL, 1);

  // Initialize timer
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, COLLECTION_DELAY, true, 0);

  dataQueue = xQueueCreate(10, sizeof(StaticJsonDocument<5000>*));  // Create queue
  if (dataQueue == NULL) {
    Serial.println("Failed to create queue");
    return;
  }
}

void loop() {
  // No actions in loop
}

// Data collection task
void CollectionDataTask(void* p) {
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  Adafruit_MPU6050 mpu;
  unsigned long timestamp = 0, counter = 0;

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (true) delay(1000);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  StaticJsonDocument<5000>* send_data = new StaticJsonDocument<5000>;

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    JsonArray entry = send_data->createNestedArray();
    entry.add(timestamp);
    pushSensorData(entry, mpu);
    pushPacketData(entry);

    counter++;
    timestamp += COLLECTION_DELAY / 1000;

    if (counter >= (1000000 / COLLECTION_DELAY)) {
      counter = 0;
      if (xQueueSend(dataQueue, &send_data, 0) != pdPASS) {
        Serial.println("Enqueue failed");
      }
      send_data = new StaticJsonDocument<5000>;
    }
  }
}

// Data sending task
void SendDataTask(void* p) {

  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi!");

  while (true) {
    if (!tcpClient.connected()) {
      Serial.println("Connecting to TCP server...");
      if (!tcpClient.connect(serverIP, serverPort)) {
        Serial.println("TCP connection failed. Retrying...");
        delay(1000);
        continue;
      }
      Serial.println("Connected to TCP server!");
    }

    StaticJsonDocument<5000>* received_data;
    if (xQueueReceive(dataQueue, &received_data, 0) == pdPASS) {
      String jsonString;
      serializeJson(*received_data, jsonString);
      delete received_data;

      if (tcpClient.connected()) {
        tcpClient.println(jsonString);
        Serial.print("Sent JSON Data: ");
        Serial.println(jsonString);
      } else {
        Serial.println("TCP disconnected, retrying...");
        tcpClient.stop();
      }
    }
  }
}

// Timer interrupt
void IRAM_ATTR onTimer() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(dataCollectionTaskHandle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

// Push sensor data
void pushSensorData(JsonArray& entry, Adafruit_MPU6050& mpu) {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  entry.add(a.acceleration.x);
  entry.add(a.acceleration.y);
  entry.add(a.acceleration.z);
}

// Push packet data
void pushPacketData(JsonArray& entry) {
  Frame* dataFrame;
  while (true) {
    if (PacketHandler::readByteWithBlocking() == ACK) {
      dataFrame = PacketHandler::readPacket();
      if (dataFrame != nullptr) break;
    }
  }

  for (int i = 0; i < dataFrame->size; i++) {
    entry.add(dataFrame->data[i]);
  }
  delete dataFrame;
}

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "PacketHandler.h"

// Pin definitions for Serial2
#define RX_PIN 18
#define TX_PIN 17
#define COLLECTION_DELAY 1000000  // Data collection interval in microseconds
#define BUFFERSIZE 1000000 / COLLECTION_DELAY

#define factory_id 0
#define device_id 0

struct SensorData {
  int f_id;
  int d_id;
  uint64_t timestamp;
  float data[6];
  /*
  acc_x
  acc_y
  acc_z
  voltage
  rpm
  temperature
  */
};

// WiFi credentials
const char* ssid = "";      // Replace with your WiFi SSID
const char* password = "";  // Replace with your WiFi password

// TCP server configuration
const char* serverIP = "";  // Replace with your server IP
const int serverPort = 3000;             // Replace with your server port

QueueHandle_t dataQueue;  // Queue to hold data for sending
Adafruit_MPU6050 mpu;     // MPU6050 sensor instance

WiFiClient tcpClient;       // TCP client for sending data
uint64_t timestamp;    // Timestamp for collected data
unsigned long counter = 0;  // Counter for batch processing


// Timer configuration
hw_timer_t* timer = NULL;                              // Hardware timer instance
TaskHandle_t dataCollectionTaskHandle = NULL;          // Task handle for data collection
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;  // Mutex for timer

void IRAM_ATTR onTimer();  // Timer interrupt handler

// Function declarations
void pushSensorData(float*, Adafruit_MPU6050&);
void pushPacketData(float*);
void CollectionDataTask(void* p);
void SendDataTask(void* p);

void setup() {
  Serial.begin(115200);                               // Start serial communication
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);  // Initialize Serial2
  Serial.println("Starting in 10 seconds...");
  delay(10000);  // Delay for setup preparation


  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nConnected to WiFi!");


  // Initialize timestamp
  configTime(0, 0, "pool.ntp.org");
  struct tm timeinfo;
  Serial.println("Waiting for NTP time sync");
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(1000);
  }
  timestamp = (uint64_t)time(nullptr);  // Initialize timestamp
  timestamp *= 1000;
  Serial.println("Time synchronized!");


  // Initialize MPU6050 sensor
  Serial.println("Connecting to MPU6050 chip");
  while (!mpu.begin()) {
    Serial.print(".");
    delay(1000);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("\nConnected to MPU6050 chip!");

  // Wait for TCP connection
  Serial.println("Connecting to TCP server");
  while (!tcpClient.connected()) {
    if (!tcpClient.connect(serverIP, serverPort)) {
      Serial.print(".");
      delay(1000);
    }
  }
  Serial.println("Connected to TCP server!");

  // Initialize timer
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, COLLECTION_DELAY, true, 0);


  // Create queue for data
  dataQueue = xQueueCreate(10, sizeof(SensorData) * BUFFERSIZE);
  if (dataQueue == NULL) {
    Serial.println("Failed to create queue");
    return;
  }

  // Create tasks
  xTaskCreatePinnedToCore(CollectionDataTask, "CollectionDataTask", 65536, NULL, 3, &dataCollectionTaskHandle, 0);
  xTaskCreatePinnedToCore(SendDataTask, "SendDataTask", 65536, NULL, 2, NULL, 1);
}

void loop() {
  // Main loop does not perform any actions
}

// Data collection task
void CollectionDataTask(void* p) {

  SensorData buffer[BUFFERSIZE];

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Wait for timer notification

    buffer[counter].f_id = factory_id;
    buffer[counter].d_id = device_id;
    buffer[counter].timestamp = timestamp;

    pushSensorData(&buffer[counter].data[0], mpu);
    pushPacketData(&buffer[counter].data[3]);

    counter++;
    timestamp += COLLECTION_DELAY / 1000;  // Increment timestamp (milliseconds)

    if (counter >= (1000000 / COLLECTION_DELAY)) {  // Batch completion condition
      counter = 0;
      if (xQueueSend(dataQueue, buffer, 0) != pdPASS) {
        Serial.println("Failed to enqueue data");
      }
    }
  }
}

// Data sending task
void SendDataTask(void* p) {
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

    SensorData buffer[BUFFERSIZE];
    if (xQueueReceive(dataQueue, buffer, portMAX_DELAY) == pdPASS) {
      if (tcpClient.connected()) {

        // 시리얼 출력: 전송 데이터를 표시
        for (int i = 0; i < BUFFERSIZE; i++) {
          Serial.print("Factory ID: ");
          Serial.println(buffer[i].f_id);
          Serial.print("Device ID: ");
          Serial.println(buffer[i].d_id);
          Serial.print("Timestamp: ");
          Serial.println(buffer[i].timestamp);
          Serial.print("Data [acc_x, acc_y, acc_z, voltage, rpm, temperature]: ");
          Serial.print(buffer[i].data[0]);
          Serial.print(", ");
          Serial.print(buffer[i].data[1]);
          Serial.print(", ");
          Serial.print(buffer[i].data[2]);
          Serial.print(", ");
          Serial.print(buffer[i].data[3]);
          Serial.print(", ");
          Serial.print(buffer[i].data[4]);
          Serial.print(", ");
          Serial.println(buffer[i].data[5]);
          Serial.println("----------------------------------------------------");
        }


        size_t dataSize = sizeof(SensorData) * BUFFERSIZE;
        tcpClient.write((uint8_t*)buffer, dataSize);  // Send data as binary
        Serial.printf("Sent %d bytes of data\n", dataSize);

      } else {
        Serial.println("TCP disconnected, retrying...");
        tcpClient.stop();
      }
    }
  }
}

// Timer interrupt handler
void IRAM_ATTR onTimer() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(dataCollectionTaskHandle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();  // Yield to a higher-priority task if necessary
  }
}

// Collect sensor data from MPU6050
void pushSensorData(float* arr, Adafruit_MPU6050& mpu) {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);  // Get sensor events

  arr[0] = a.acceleration.x;
  arr[1] = a.acceleration.y;
  arr[2] = a.acceleration.z;
}

// Collect additional packet data
void pushPacketData(float* arr) {
  Frame* dataFrame;
  while (true) {
    if (PacketHandler::readByteWithBlocking() == ACK) {
      dataFrame = PacketHandler::readPacket();
      if (dataFrame != nullptr) break;  // Valid packet received
    }
  }

  for (int i = 0; i < 3; i++) {
    arr[i] = dataFrame->data[i];  // Add packet data to entry
  }
  delete dataFrame;  // Free memory
}

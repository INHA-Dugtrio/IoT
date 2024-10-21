#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <HardwareSerial.h> 
#include "PacketHandler.h"
#include "Frame.h"
#define TX_PIN 17
#define RX_PIN 18

Adafruit_MPU6050 mpu;
const char* url = "http://165.246.43.228:3000/api";
const char* ssid = "cnlab";  // Wi-Fi SSID
const char* password = "cnlab1110";  // Wi-Fi Password

Frame* sendMoreData();

void setup(){
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    
    Serial.println("Connected to WiFi!");

    if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }

  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop(){
    if(PacketHandler::readByteWithBlocking() == ACK){
        Frame* data_frame = PacketHandler::readPacket();
        if(data_frame != nullptr){
            PacketHandler::printPacket(data_frame);
            PacketHandler::sendPacket(data_frame, url);
            delete data_frame;
        }
    }
}

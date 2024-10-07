#include <HardwareSerial.h> 
#include "PacketHandler.h"
#include "Frame.h"
#define RX_PIN 17
#define TX_PIN 16

//HardwareSerial SerialPLC(2);

void setup(){
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
}

void loop(){
    if(PacketHandler::readByteWithBlocking() == ACK){
        Frame* data_frame = PacketHandler::readPacket();
        if(data_frame != nullptr){
            PacketHandler::printPacket(data_frame);
            PacketHandler::sendPacket(data_frame);
            delete(data_frame);
        }
    }
}

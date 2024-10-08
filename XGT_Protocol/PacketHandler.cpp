#include "PacketHandler.h"
#include <HardwareSerial.h>
#define ACK 0x06
#define EXT 0x03
#define MAX 256
#define DELAY_MS 100


unsigned char PacketHandler::readByteWithBlocking(){
    while (Serial2.available() <= 0) {}  // Polling 
    return Serial2.read();  // 1 Byte 읽기
}

// XGT Protocol 기반으로 데이터 추출
Frame* PacketHandler::readPacket(){
    int buffer_size = 0;
    unsigned char buffer[MAX];

    while(true){
        if(buffer_size > MAX){
            Serial.println("Error: Buffer overflow");
            return nullptr;
        }
        else{
            unsigned char input = readByteWithBlocking();
            if (input == EXT) break;  // Tail
            buffer[buffer_size++] = input;
        }
    }

    Frame* data_frame = nullptr;
    if(calculateBCC(buffer, buffer_size) == readBCC()){
      // BCC Check Success
      int* data = new int[buffer_size/4];
      data_frame = new Frame(data, (buffer_size-8) / 4);

      int data_index = 0;
      int buffer_index = 9; // XGT Protocol 데이터 시작 부분
      while(buffer_index < buffer_size){
          data[data_index] =  (asciiHexToDecimal(buffer[buffer_index + 2]) << 12) |
                              (asciiHexToDecimal(buffer[buffer_index + 3]) << 8) |
                              (asciiHexToDecimal(buffer[buffer_index + 0]) << 4) |
                              (asciiHexToDecimal(buffer[buffer_index + 1]) << 0);
          data_index++;
          buffer_index += 4;
      }
    }
    else{
      // BCC Check Fail
      Serial.println("Error: BCC mismatch");
    }

    return data_frame;
}

void PacketHandler::printPacket(Frame* data_frame){
    for(int i = 0; i < data_frame->size; i++){
        Serial.print(data_frame->data[i]);
        Serial.print(' ');
    }
    Serial.println();
}

void PacketHandler::sendPacket(Frame* data_frame){
    // do nothing
}

// Ascii 16진수 -> 정수 10진수
int PacketHandler::asciiHexToDecimal(char c){
    int dec;
    if (c >= '0' && c <= '9') {
        dec = (c - '0');
    } 
    else if (c >= 'A' && c <= 'F') {
        dec = (c - 'A' + 10);
    }
    return dec;
}

// BCC 코드 읽기
int PacketHandler::readBCC(){
    char input[2];
    int BCC_value = 0;

    input[0] = readByteWithBlocking();
    input[1] = readByteWithBlocking();

    BCC_value += asciiHexToDecimal(input[0]) * 16;
    BCC_value += asciiHexToDecimal(input[1]);

    return BCC_value;
}

// 패킷에 대한 BCC 계산
int PacketHandler::calculateBCC(unsigned char buffer[], int size){
    int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return (sum % 256) + ACK + EXT; // ACK, EXT 포함
}
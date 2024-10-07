#include "PacketHandler.h"
#include <HardwareSerial.h>
#define ACK 0x06
#define EXT 0x03
#define MAX 256
#define DELAY_MS 1000


// ���� ������� ��ٷȴٰ� 1 ����Ʈ �о����
unsigned char PacketHandler::readByteWithBlocking(){
    while (Serial2.available() <= 0) {}  // ������ ���
    return Serial2.read();  // ���� ����Ʈ ��ȯ
}

// XGT �������ݷ� ������ �������� ������ �б�
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
            if (input == EXT) break;  // ��Ŷ ���� ��ȣ
            buffer[buffer_size++] = input;
        }
    }

    Frame* data_frame = nullptr;
    if(calculateBCC(buffer, buffer_size) == readBCC()){
        int* data = new int[buffer_size/4];
        data_frame = new Frame(data, (buffer_size-8) / 4);

        int data_index = 0;
        int buffer_index = 9; // XGT Protocol ��Ŷ�� ������ ���� �κ�
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

// �ƽ�Ű 16������ 10������ ��ȯ
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

// BCC �ڵ� �б�
int PacketHandler::readBCC(){
    char input[2];
    int BCC_value = 0;

    // ���� 4��Ʈ (16���� ù �ڸ�)
    input[0] = readByteWithBlocking();
    // ���� 4��Ʈ (16���� �� ��° �ڸ�)
    input[1] = readByteWithBlocking();

    BCC_value += asciiHexToDecimal(input[0]) * 16;
    BCC_value += asciiHexToDecimal(input[1]);

    return BCC_value;
}

// ��Ŷ�� BCC ���
int PacketHandler::calculateBCC(unsigned char buffer[], int size){
    int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return (sum % 256) + ACK + EXT; // ACK �� EXT �� �����Ͽ� ���
}
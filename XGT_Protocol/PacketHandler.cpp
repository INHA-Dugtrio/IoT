#include "PacketHandler.h"
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#define ACK 0x06
#define EXT 0x03
#define MAX 256 


unsigned char PacketHandler::readByteWithBlocking() {
	while (Serial2.available() <= 0) {}  // Polling 
	return Serial2.read();  // 1 Byte 읽기
}

// XGT Protocol 기반으로 데이터 추출
Frame* PacketHandler::readPacket() {
	int buffer_size = 0;
	unsigned char buffer[MAX];

	while (true) {
		if (buffer_size > MAX) {
			Serial.println("Error: Buffer overflow");
			return nullptr;
		}
		else {
			unsigned char input = readByteWithBlocking();
			if (input == EXT) break;  // Tail
			buffer[buffer_size++] = input;
		}
	}

	Frame* data_frame = nullptr;
	if (calculateBCC(buffer, buffer_size) == readBCC()) {
		// BCC Check Success
		float* data = new float[buffer_size / 8];
		data_frame = new Frame(data, (buffer_size - 8) / 8);

		int data_index = 0;
		int buffer_index = 9; // XGT Protocol 데이터 시작 부분
		while (buffer_index < buffer_size) {
      // 리틀 엔디안에서 빅 엔디안 순서로 32비트 값을 조립
      uint32_t float_bits =  (asciiHexToDecimal(buffer[buffer_index + 6]) << 28) |  // '41'
                           (asciiHexToDecimal(buffer[buffer_index + 7]) << 24) |  // 'D1'
                           (asciiHexToDecimal(buffer[buffer_index + 4]) << 20) |  // '00'
                           (asciiHexToDecimal(buffer[buffer_index + 5]) << 16) |  // '00'
                           (asciiHexToDecimal(buffer[buffer_index + 2]) << 12) |  // '00'
                           (asciiHexToDecimal(buffer[buffer_index + 3]) << 8)  |  // '00'
                           (asciiHexToDecimal(buffer[buffer_index + 0]) << 4)  |  // '00'
                           (asciiHexToDecimal(buffer[buffer_index + 1]) << 0);    // '00'

      // 32비트 값을 float로 변환
      float float_data;
      memcpy(&float_data, &float_bits, sizeof(float_data));

      // 변환된 float 값을 배열에 저장
      data[data_index] = float_data;

      // 인덱스 증가
      data_index++;
      buffer_index += 8;  // 8개의 ASCII 문자(4바이트) 처리 후 다음 값으로 이동
    }
	}
	else {
		// BCC Check Fail
		Serial.println("Error: BCC mismatch");
	}

	return data_frame;
}

void PacketHandler::printPacket(Frame* data_frame) {
	for (int i = 0; i < data_frame->size; i++) {
		Serial.print(data_frame->data[i]);
		Serial.print(' ');
	}
	Serial.println();
}

void PacketHandler::sendPacket(Frame* data_frame, const char* url) {
	if (WiFi.status() == WL_CONNECTED) {
		HTTPClient http;
		http.begin(url);
		http.addHeader("Content-Type", "application/json");

		// JSON 객체 생성
		StaticJsonDocument<200> jsonDoc;
		JsonArray dataArray = jsonDoc.createNestedArray("data");

		// Frame 객체의 data 배열을 JSON 배열로 변환
		for (int i = 0; i < data_frame->size; i++) {
			dataArray.add(data_frame->data[i]);
		}

		// JSON 객체를 문자열로 변환
		String jsonString;
		serializeJson(jsonDoc, jsonString);

		// HTTP POST 요청으로 JSON 전송
		int httpResponseCode = http.POST(jsonString);

		// 응답 코드 출력
		if (httpResponseCode > 0) {
			String response = http.getString();
			Serial.println("HTTP Response code: " + String(httpResponseCode));
			Serial.println("Response: " + response);
		}
		else {
			Serial.println("Error on sending POST: " + String(httpResponseCode));
		}

		// 요청 종료
		http.end();
	}
	else {
		Serial.println("WiFi not connected");
	}

}

// Ascii 16진수 -> 정수 10진수
int PacketHandler::asciiHexToDecimal(char c) {
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
int PacketHandler::readBCC() {
	char input[2];
	int BCC_value = 0;

	input[0] = readByteWithBlocking();
	input[1] = readByteWithBlocking();

	BCC_value += asciiHexToDecimal(input[0]) * 16;
	BCC_value += asciiHexToDecimal(input[1]);

	return BCC_value;
}

// 패킷에 대한 BCC 계산
int PacketHandler::calculateBCC(unsigned char buffer[], int size) {
	int sum = 0;
	for (int i = 0; i < size; i++) {
		sum += buffer[i];
	}
	return (sum + ACK + EXT) % 256; // ACK, EXT 포함
}
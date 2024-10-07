#define DETECT 5  // 감지 핀
#define DAC_PIN 25  // DAC 출력 핀
unsigned long lastDetectionTime = 0;  // 마지막 감지 시각
unsigned long currentTime = 0;  // 현재 시각
float rpm = 0;  // RPM 값 저장

void setup() {
  Serial.begin(115200);
  pinMode(DETECT, INPUT_PULLUP);  // 풀업 저항을 사용한 안정적 입력

  // 감지 시 인터럽트 설정
  attachInterrupt(digitalPinToInterrupt(DETECT), sensorTriggered, FALLING);  // FALLING 에지에서 인터럽트 발생
}

void loop() {
  // RPM 값을 DAC 출력으로 매핑 (0 RPM -> 0V, 3000 RPM -> 5V)
  int dacValue = map(rpm, 0, 3000, 0, 255);  // 0-3000 RPM을 0-255로 매핑
  dacValue = constrain(dacValue, 0, 255);  // 값을 0-255 사이로 제한
  
  dacWrite(DAC_PIN, dacValue);  // DAC에 출력

  // 현재 RPM 값을 출력
  Serial.print("RPM: ");
  Serial.print(rpm);
  Serial.print(" | DAC Value: ");
  Serial.println(dacValue);

  delay(500);  // 0.5초마다 출력
}

// 레이저 감지 시 호출되는 함수 (인터럽트 서비스 루틴)
void sensorTriggered() {
  currentTime = millis();  // 현재 시각을 기록

  // 이전 감지 시각과의 시간 차이 계산 (ms 단위)
  unsigned long timeDifference = currentTime - lastDetectionTime;

  // 시간 차이가 0이 아닐 경우 (나누기 0 방지)
  if (timeDifference > 0) {
    // RPM 계산: 60000ms(1분) / 감지된 두 지점 사이의 시간 차이
    rpm = 60000.0 / timeDifference;
  }

  // 마지막 감지 시각 업데이트
  lastDetectionTime = currentTime;
}

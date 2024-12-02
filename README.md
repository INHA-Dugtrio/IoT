# DataCollection 코드 설명

## 개요
`DataCollection` 코드는 **ESP32**를 기반으로 센서 데이터를 수집하고, **PLC-HMI 통신 감청** 데이터를 하나로 결합하여 **WiFi**를 통해 서버로 전송하는 역할을 수행합니다. 이 코드는 정확한 실시간 데이터 수집을 위해 **멀티 태스크** 구조로 프로그래밍되어 있으며, 센서 데이터와 감청 데이터를 하나의 패킷으로 만들어 서버로 전송하는 핵심 기능을 담당합니다.

### 주요 특징:
- **멀티 태스킹**: 실시간으로 데이터를 수집하고 전송하기 위해 별도의 작업(Task)을 사용하여 데이터 수집과 전송 작업이 병렬로 수행됩니다.
- **PLC-HMI 통신 감청**: PLC와 HMI 간의 통신을 감청하여 데이터를 분석하고 이를 서버로 전송합니다.
- **가속도 센서 데이터 수집**: 가속도 센서는 **ESP32**에 직접 연결되어 데이터를 읽어오며, 이는 PLC에 연결되지 않고 ESP32에서 실시간으로 처리됩니다.

## 주요 기능

1. **센서 데이터 수집**
   - **MPU6050** 같은 가속도 센서를 통해 **X, Y, Z축** 가속도 데이터를 수집합니다.
   - 가속도 센서는 **ESP32**에 직접 연결되어 있으며, **PLC-HMI 통신을 통해 감청되지 않습니다**.
   - 센서 데이터를 주기적으로 읽어와서 타임스탬프와 함께 저장합니다.
   - 수집된 데이터는 구조체에 저장되어 전송됩니다.

2. **통신 데이터 감청**
   - PLC와 HMI 간의 통신을 감청하여 **XGT Protocol**을 통해 전송되는 데이터를 추출합니다.
   - 감청한 데이터를 센서 데이터와 결합하여 하나의 패킷으로 만듭니다.

3. **WiFi를 통한 서버 전송**
   - 수집된 데이터는 WiFi를 통해 지정된 서버로 전송됩니다.
   - 데이터는 TCP/IP 기반 소켓 통신을 사용하여 서버로 전달되며, 전송 결과는 로그로 출력됩니다.

## 코드 구조

### 1. `setup()`
- 초기화 함수로, 시리얼 통신을 시작하고 WiFi에 연결합니다.
- **두 개의 주요 작업(Task)**을 생성:
  - **데이터 수집 작업 (CollectionDataTask)**: 센서와 감청 데이터를 수집하는 작업.
  - **데이터 전송 작업 (SendDataTask)**: 수집된 데이터를 주기적으로 서버로 전송하는 작업.
- 타이머를 설정하여 주기적으로 데이터 수집을 트리거합니다.

### 2. `loop()`
- 이 함수는 빈 상태로 유지됩니다. 실제 작업은 별도의 작업(Task)에서 실행됩니다.

### 3. `CollectionDataTask`
- 센서 데이터를 읽어오고, 감청한 통신 데이터를 함께 결합하는 작업을 수행합니다.
- 주기적으로 타이머 인터럽트에 의해 호출됩니다.
- **구체적인 과정**:
  1. **센서 데이터 수집**: MPU6050에서 가속도 데이터를 읽어옵니다. 가속도 데이터는 ESP32에 직접 연결되어 수집됩니다.
  2. **통신 데이터 감청**: XGT Protocol로부터 감청 데이터를 가져와 센서 데이터와 결합합니다.
  3. **데이터 전송 준비**: 수집된 데이터를 구조체에 저장하여 전송 대기 상태로 만듭니다.

### 4. `SendDataTask`
- 주기적으로 서버로 데이터를 전송합니다.
- **전송 과정**:
  1. **WiFi 연결 확인**: WiFi가 연결되지 않았을 경우, 다시 연결을 시도합니다.
  2. **데이터 전송**: 구조체에 저장된 데이터를 TCP/IP 기반 소켓 통신으로 전송합니다.

### 5. `pushSensorData()`
- **ESP32**에 연결된 **MPU6050**에서 가속도 데이터를 읽어와 구조체에 추가하는 함수입니다.
- 가속도 데이터를 수집하고 이를 전송할 패킷에 포함시킵니다.

### 6. `pushPacketData()`
- 감청된 통신 데이터를 구조체에 추가하는 함수입니다.
- **XGT Protocol**을 분석하여 추출한 데이터를 패킷에 포함시킵니다.

## 사용 방법

### 1. 요구 사항
- **하드웨어**:
  - ESP32
  - MPU6050 센서 (ESP32에 직접 연결)
  - PLC-HMI 시스템 (감청 대상)
- **소프트웨어**:
  - Arduino IDE
  - WiFi 네트워크

### 2. 실행 방법
1. Arduino IDE에서 코드를 작성한 후 ESP32에 업로드합니다.
2. ESP32가 WiFi에 연결되고, PLC-HMI 통신을 감청하며 센서 데이터를 수집하기 시작합니다.
3. 수집된 데이터는 서버로 주기적으로 전송됩니다.

### 3. 서버 설정
- 데이터를 수신할 서버는 TCP/IP 기반 소켓 통신을 처리할 수 있어야 합니다.
- 서버 주소와 포트는 실제 서버 정보에 맞게 수정해야 합니다.

## 데이터 전송 형식
데이터 구성:
- 첫 번째 데이터: timestamp (데이터가 수집된 시간).
- 두 번째 데이터부터 네 번째 데이터까지: sensor_data (MPU6050 센서의 가속도 값).
    - x: X축 가속도 값.
    - y: Y축 가속도 값.
    - z: Z축 가속도 값.
- 그 이후 데이터: packet_data (감청된 실수 데이터로, 개수는 유동적).
이 형식은 서버로 전송될 때 구조체로 저장되며, 배열의 첫 번째 값은 timestamp, 그 다음은 sensor_data 및 packet_data가 순서대로 포함됩니다. packet_data의 개수는 상황에 따라 달라질 수 있습니다.

서버로 전송되는 데이터는 구조체로 다음과 같이 저장됩니다:

factory_id    : int    
device_id     : int    
timestamp     : uint64   
vibration_x   : float   
vibration_y   : float   
vibration_z   : float   
voltage       : float   
rpm           : float   
temperature   : float   

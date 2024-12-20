#ifndef PH_H
#define PH_H

#include <HardwareSerial.h>
#define ACK 0x06
#define EXT 0x03
#define MAX 256

class Frame {
public:
  Frame(float* _data, int _size)
    : data(_data), size(_size) {}
  ~Frame() {
    delete data;
  }
  int size;
  float* data;
};


class PacketHandler {
public:

  static unsigned char readByteWithBlocking();
  static Frame* readPacket();

private:

  static int asciiHexToDecimal(char);
  static int readBCC();
  static int calculateBCC(unsigned char[], int);
};

#endif
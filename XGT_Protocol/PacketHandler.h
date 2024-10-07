#ifndef PH_H
#define PH_H

#include "Frame.h"
#define ACK 0x06
#define EXT 0x03
#define MAX 256

class PacketHandler{
public:

    static unsigned char readByteWithBlocking();
    static Frame* readPacket();
    static void printPacket(Frame*);
    static void sendPacket(Frame*);

private:

    static int asciiHexToDecimal(char);
    static int readBCC();
    static int calculateBCC(unsigned char[], int);

};

#endif
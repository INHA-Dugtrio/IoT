#ifndef FRAME_H
#define FRAME_H

class Frame{
public:
    Frame(int*, int);
    ~Frame();
    int size;
    int* data;
};

#endif
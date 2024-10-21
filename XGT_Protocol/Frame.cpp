#include "Frame.h"


Frame::Frame(float* _data, int _size) : data(_data), size(_size) {}

Frame::~Frame() { delete data; }
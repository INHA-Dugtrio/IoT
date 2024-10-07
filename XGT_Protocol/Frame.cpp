#include "Frame.h"


Frame::Frame(int* _data, int _size) : data(_data), size(_size) {}

Frame::~Frame(){ delete(data); }
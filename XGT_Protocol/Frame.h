#ifndef FRAME_H
#define FRAME_H

class Frame {
public:
	Frame(float*, int);
	~Frame();
	int size;
	float* data;
};

#endif
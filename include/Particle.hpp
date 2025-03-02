#pragma once

#include <cstdint>

class Particle {
public :
	float position[2];
	float speed[2];
	float acceleration[2];
	uint8_t colour[4];

	void print();
};
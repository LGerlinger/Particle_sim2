#pragma once

#include <cstdint>

class Particle {
public :
	float position[2];
	float speed[2];
	// float acceleration[2];
	// uint8_t colour[4];

	/**
	* @brief Changes colour to show whether the particle is selected or not
	*/
	void select(bool selection);

	/**
	* @brief Prints the particle's position, speed and acceleration 
	*/
	void print();
};
#include <iostream>

#include "Particle.hpp"

void Particle::select(bool selection) {
	// colour[3] = selection ? 100 : 255;
}

void Particle::print() {
	std::cout << "    position[" << position[0] << ", " << position[1] << "]" << std::endl;
	std::cout << "       speed[" << speed[0] << ", " << speed[1] << "]" << std::endl;
}
#include <iostream>

#include "Particle.hpp"

void Particle::print() {
	std::cout << "    position[" << position[0] << ", " << position[1] << "]" << std::endl;
	std::cout << "       speed[" << speed[0] << ", " << speed[1] << "]" << std::endl;
	std::cout << "acceleration[" << acceleration[0] << ", " << acceleration[1] << "]" << std::endl;
}
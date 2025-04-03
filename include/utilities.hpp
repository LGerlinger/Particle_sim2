#pragma once

#include <iostream>
#include <string>

template<typename T>
void print_vect(std::string name, T vect[2]) {
	std::cout << name << " [" << vect[0] << ", " << vect[1] << "]" << std::endl;
}
inline void print_vect(std::string name, uint8_t vect[2]) {
	std::cout << name << " [" << (short)vect[0] << ", " << (short)vect[1] << "]" << std::endl;
}
inline void print_vect(std::string name, int8_t vect[2]) {
	std::cout << name << " [" << (short)vect[0] << ", " << (short)vect[1] << "]" << std::endl;
}
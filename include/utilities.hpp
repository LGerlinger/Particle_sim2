#pragma once

#include <iostream>
#include <string>

template<typename T>
void print_vect(std::string name, T vect[2]) {
	std::cout << name << " [" << vect[0] << ", " << vect[1] << "]" << std::endl;
}
inline void print_vect(std::string name, uint8_t vect[2]) {
	std::cout << name << " [" << (unsigned short)vect[0] << ", " << (unsigned short)vect[1] << "]" << std::endl;
}
inline void print_vect(std::string name, int8_t vect[2]) {
	std::cout << name << " [" << (short)vect[0] << ", " << (short)vect[1] << "]" << std::endl;
}

inline std::string b2s(bool b) {return b ? "true" : "false";}

inline std::string i2s(unsigned long number) {
	std::string str = std::to_string(number);
	int n = str.length() - 3;
	while (n > 0) {
		str.insert(n, " ");
		n -= 3;
	}
	return str;
}
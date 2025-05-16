#pragma once

#include <cstdint>
#include <vector>

struct GridCoord {
	uint16_t coord[2];

	GridCoord() = default;
	GridCoord(uint16_t x, uint16_t y) : coord{x, y} {};
	// GridCoord(GridCoord& gc) : coord{gc[0], gc[1]} {};
	inline uint16_t operator[](bool b) {return coord[b];};
};

class Segment {
public :
	float pos[2][2]; //< Position of both end

	// These members are redondant and can be calculated from pos. However doing so at each iteration is unefficient when segments don't even move.
	float vect_norm[2]; //< Normalised directionnal vector
	float min[2]; //< Minimum position x, y
	float max[2]; //< Maximum position x, y

	std::vector<GridCoord> cells;

	Segment(float Ax, float Ay, float Bx, float By);
	// void initialize(float Ax, float Ay, float Bx, float By);
	void printSelf();
};
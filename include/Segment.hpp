#pragma once

#include "GridCoord.hpp"

#include <vector>

class Segment {
public :
	float pos[2][2]; //< Position of both end

	// These members are redondant and can be calculated from pos. However doing so at each iteration is inefficient when segments don't even move.
	float vect_norm[2]; //< Normalised directionnal vector
	float min[2]; //< Minimum position x, y
	float max[2]; //< Maximum position x, y

	std::vector<GridCoord> cells;

	Segment(float Ax, float Ay, float Bx, float By);
	// void initialize(float Ax, float Ay, float Bx, float By);
	void printSelf();
};
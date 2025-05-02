#include "Segment.hpp"
#include <cmath>

void Segment::initialize(float Ax, float Ay, float Bx, float By) {
	pos[0][0] = Ax;
	pos[0][1] = Ay;
	pos[1][0] = Bx;
	pos[1][1] = By;

	vect_norm[0] = Bx - Ax;
	vect_norm[1] = By - Ay;
	float norm = sqrt(vect_norm[0]*vect_norm[0] + vect_norm[1]*vect_norm[1]);
	vect_norm[0] /= norm;
	vect_norm[1] /= norm;

	min[0] = std::min(Ax, Bx);
	max[0] = std::max(Ax, Bx);
	min[1] = std::min(Ay, By);
	max[1] = std::max(Ay, By);
}
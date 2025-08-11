#pragma once

#include "GridCoord.hpp"

class Rectangle {
private :
	float pos_[2];
	float size_[2];

public :
	inline Rectangle() = default;
	inline Rectangle(const Rectangle& copy) {
		 pos_[0] = copy.pos_[0];   pos_[1] = copy.pos_[1];
		size_[0] = copy.size_[0]; size_[1] = copy.size_[1];
	};
	inline Rectangle(float posX, float posY, float sizeX, float sizeY) {
		 pos_[0] = posX;   pos_[1] = posY;
		size_[0] = sizeX; size_[1] = sizeY;
	};

	inline float* getMem() {return pos_;};

	inline float pos(bool coord) {return pos_[coord];};
	inline float size(bool coord) {return size_[coord];};
	inline float endPos(bool coord) {return pos_[coord] + size_[coord];};

	inline bool check_in(float obj_pos[2]) {
		return pos_[0] <= obj_pos[0] && obj_pos[0] <= endPos(0) &&
		       pos_[1] <= obj_pos[1] && obj_pos[1] <= endPos(1);
	};
};

/**
* A Rectangle that also has its lower and upper bounds in the world grid.
* A Zone has a function (here a function number) that defines its interaction with Particles. 
*/
class Zone : public Rectangle {
private :
	GridCoord gridPos[2]; //< Position in the grid being [(lowX,lowY), (upX,upY)[, (upper is excluded).
	bool length_coord; //< Whether the length is along the X axis (value at 0) or the Y axis (1).
public :
	/*
	Eventually, I should change fun to a fonction pointer so that the Zone can 1) be given any function to call, 2) have its own arguments.
	But for now the Particle simulator will use fun in a switch to know which function to call and give it its arguments.
	*/
	int8_t fun; //< Choose which function will be called when a Particle enters the Zone with the member fun.
	inline bool lc() {return length_coord;}; 

	Zone(int8_t fun_, float rect[2][2], GridCoord gridCoord[2]) :
		fun(fun_),
		Rectangle(rect[0][0], rect[0][1], rect[1][0], rect[1][1]),
		gridPos{gridCoord[0], gridCoord[1]},
		length_coord(gridCoord[1][0] - gridCoord[0][0] < gridCoord[1][1] - gridCoord[0][1]) {};

	Zone(int8_t fun_, Rectangle& rect, GridCoord gridCoord[2]) :
		fun(fun_),
		Rectangle(rect),
		gridPos{gridCoord[0], gridCoord[1]},
		length_coord(gridCoord[1][0] - gridCoord[0][0] < gridCoord[1][1] - gridCoord[0][1]) {};


	// All these get functions return a 1D grid coordinate.

	inline uint16_t getWidthLowBound()  {return gridPos[0][!length_coord];};
	inline uint16_t getWidthUpBound()   {return gridPos[1][!length_coord];};
	inline uint16_t getLengthLowBound() {return gridPos[0][ length_coord];};
	inline uint16_t getLengthUpBound()  {return gridPos[1][ length_coord];};

	inline uint16_t getLength() {return gridPos[1][ length_coord] - gridPos[0][ length_coord];};
	inline uint16_t getWIdth()  {return gridPos[1][!length_coord] - gridPos[0][!length_coord];};
};
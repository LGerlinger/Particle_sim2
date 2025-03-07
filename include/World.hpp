#pragma once

#include "Particle.hpp"
#include "Segment.hpp"

#include <cstdint>

#define MAX_PART_CELL 4
#define MAX_SEG_CELL 4

struct Cell {
	uint8_t nb_parts;
	uint8_t nb_segs;
	uint32_t parts[MAX_PART_CELL];
	uint32_t segs[MAX_SEG_CELL];
};

class World {
public :
	float size[2];

	uint16_t gridSize[2];
	float cellSize[2];
	
	Cell* grid;
	inline Cell& getCell(uint16_t x, uint16_t y) {return grid[y*gridSize[0] +x];};
	Cell* getCell_fromPos(float pos_x, float pos_y);
	bool getCellCoord_fromPos(float pos_x, float pos_y, uint16_t* x, uint16_t* y);

	World(float sizeX, float sizeY, float cellSizeX, float cellSizeY);

	void update_grid_particle_contenance(Particle* particle_array, uint32_t array_size);
	void update_grid_segment_contenance(Segment* segment_array, uint32_t array_size);

	void change_cell_part(uint32_t part, float init_pos_x, float init_pos_y, float end_pos_x, float end_pos_y);
};
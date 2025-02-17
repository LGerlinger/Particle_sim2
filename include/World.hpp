#pragma once

#include "Particle.hpp"
#include "Segment.hpp"
#include <cstdint>
#include <vector>

struct Cell {
	uint8_t nb_parts;
	uint8_t nb_segs;
	uint32_t parts[50];
	uint32_t segs[20];
};

class World {
public :
	float size[2];

	uint16_t gridSize[2];
	float cellSize[2];
	Cell* grid;
	inline Cell& getCell(uint16_t x, uint16_t y) {return grid[y*gridSize[0] +x];};

	World(float sizeX, float sizeY, float gridSizeX, float gridSizeY);

	void update_grid_particle_contenance(Particle* particle_array, uint32_t array_size);
	void update_grid_segment_contenance(Segment* segment_array, uint32_t array_size);
};
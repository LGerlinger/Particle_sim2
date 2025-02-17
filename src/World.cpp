#include "World.hpp"
#include <cstdlib>
#include <iostream>

World::World(float sizeX, float sizeY, float gridSizeX, float gridSizeY) {
	std::cout << "World::World : Size [" << sizeX << ", " << sizeY << "]   gridSize [" << gridSizeX << ", " << gridSizeY << "]" << std::endl;
	size[0] = sizeX;
	size[1] = sizeY;
	gridSize[0] = gridSizeX;
	gridSize[1] = gridSizeY;
	cellSize[0] = sizeX / gridSizeX;
	cellSize[1] = sizeY / gridSizeY;
	std::cout << "cellSize :[" << cellSize[0] << ", " << cellSize[1] << "]" << std::endl;

	grid = (Cell*)malloc(gridSizeX*gridSizeY* sizeof(Cell));
}


void World::update_grid_particle_contenance(Particle* particle_array, uint32_t array_size) {
	// std::cout << "World::update_grid_particle_contenance" << std::endl;
	// "Emptying" cells
	for (uint16_t y=0; y<gridSize[1]; y++) {
		for (uint16_t x=0; x<gridSize[0]; x++) {
			getCell(x, y).nb_parts = 0;
		}
	}

	// Filling them
	for (uint32_t i=0; i<array_size; i++) {
		if (0 <= particle_array[i].position[0] && particle_array[i].position[0] < size[0] &&
				0 <= particle_array[i].position[1] && particle_array[i].position[1] < size[1]) {
			// std::cout << "particle " << i << " pos=[" << particle_array[i].position[0] << ", " << particle_array[i].position[1] << "] ,\taccessing [" << (uint16_t)(particle_array[i].position[0]/cellSize[0]) << ", " << (uint16_t)(particle_array[i].position[1]/cellSize[1]) << "]" << std::endl;
			Cell& cell = getCell((uint16_t)(particle_array[i].position[0]/cellSize[0]), (uint16_t)(particle_array[i].position[1]/cellSize[1]));
			cell.parts[cell.nb_parts++] = i;
		}
	}
}


void World::update_grid_segment_contenance(Segment* segment_array, uint32_t array_size) {
	// "Emptying" cells
	for (uint16_t y=0; y<gridSize[1]; y++) {
		for (uint16_t x=0; x<gridSize[0]; x++) {
			getCell(x, y).nb_segs = 0;
		}
	}

	float vec[2];
	for (uint32_t i=0; i<array_size; i++) {
		vec[0] = segment_array[i].pos[1][0] - segment_array[i].pos[0][0];
		vec[1] = segment_array[i].pos[1][1] - segment_array[i].pos[0][1];

		if (vec[0] == 0) { // vertical line
			uint16_t min = segment_array[i].pos[0][1] < segment_array[i].pos[1][1] ? segment_array[i].pos[0][1] : segment_array[i].pos[1][1];
			uint16_t max = segment_array[i].pos[0][1] > segment_array[i].pos[1][1] ? segment_array[i].pos[0][1] : segment_array[i].pos[1][1];
			uint16_t x = segment_array[i].pos[0][0] / cellSize[0];
			for (uint16_t y=min; y<max; y++) {
				getCell(x, y).segs[getCell(x, y).nb_segs++] = i;
			}
		}
		else if (vec[1] == 0) { // horizontal line
			uint16_t min = segment_array[i].pos[0][0] < segment_array[i].pos[1][0] ? segment_array[i].pos[0][0] : segment_array[i].pos[1][0];
			uint16_t max = segment_array[i].pos[0][0] > segment_array[i].pos[1][0] ? segment_array[i].pos[0][0] : segment_array[i].pos[1][0];
			uint16_t y = segment_array[i].pos[0][1] / cellSize[1];
			for (uint16_t x=min; x<max; x++) {
				getCell(x, y).segs[getCell(x, y).nb_segs++] = i;
			}
		}
		else {

		}
	}
}
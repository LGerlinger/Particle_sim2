#include "World.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

World::World(float sizeX, float sizeY, float cellSizeX, float cellSizeY) {
	std::cout << "World::World : Size [" << sizeX << ", " << sizeY << "]   cellSize [" << cellSizeX << ", " << cellSizeY << "]" << std::endl;
	size[0] = sizeX;
	size[1] = sizeY;
	cellSize[0] = cellSizeX;
	cellSize[1] = cellSizeY;
	gridSize[0] = ceil(sizeX / cellSizeX);
	gridSize[1] = ceil(sizeY / cellSizeY);
	std::cout << "gridSize :[" << gridSize[0] << ", " << gridSize[1] << "]" << std::endl;

	grid = (Cell*)malloc(gridSize[0]*gridSize[1]* sizeof(Cell));
}


Cell* World::getCell_fromPos(float pos_x, float pos_y) {
	if (0 <= pos_x && pos_x <= size[0] &&
	    0 <= pos_y && pos_y <= size[1]) {
		return &grid[(uint16_t)(pos_y/cellSize[1])*gridSize[0] + (uint16_t)(pos_x/cellSize[0])];
	}
	else return nullptr;
}


bool World::getCellCoord_fromPos(float pos_x, float pos_y, uint16_t* x, uint16_t* y) {
	if (0 <= pos_x && pos_x <= size[0] &&
	    0 <= pos_y && pos_y <= size[1]) {
			*x = (uint16_t)(pos_x/cellSize[0]);
			*y = (uint16_t)(pos_y/cellSize[1]);
		return true;
	}
	else return false;
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
			cell.parts[cell.nb_parts] = i;
			cell.nb_parts = (cell.nb_parts+1) %MAX_PART_CELL;
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
				Cell& cell = getCell(x, y);
				cell.segs[getCell(x, y).nb_segs] = i;
				cell.nb_segs = (cell.nb_segs+1) % MAX_SEG_CELL;
			}
		}
		else if (vec[1] == 0) { // horizontal line
			uint16_t min = segment_array[i].pos[0][0] < segment_array[i].pos[1][0] ? segment_array[i].pos[0][0] : segment_array[i].pos[1][0];
			uint16_t max = segment_array[i].pos[0][0] > segment_array[i].pos[1][0] ? segment_array[i].pos[0][0] : segment_array[i].pos[1][0];
			uint16_t y = segment_array[i].pos[0][1] / cellSize[1];
			for (uint16_t x=min; x<max; x++) {
				Cell& cell = getCell(x, y);
				cell.segs[getCell(x, y).nb_segs] = i;
				cell.nb_segs = (cell.nb_segs+1) % MAX_SEG_CELL;
			}
		}
		else {
			// this needs implementing
		}
	}
}


void World::change_cell_part(uint32_t part, float init_pos_x, float init_pos_y, float end_pos_x, float end_pos_y) {
	Cell* cell = getCell_fromPos(init_pos_x, init_pos_y);
	if (cell) {
		uint8_t foundAt = -1;
		for (uint8_t i=0; i<cell->nb_parts; i++) {
			if (cell->parts[i] == part){
				foundAt = i;
				break;
			}
		}
		if (foundAt != (uint8_t)-1) {
			for (uint8_t i=foundAt+1; i<cell->nb_parts; i++) {
				cell->parts[i-1] = cell->parts[i];
			}
			cell->nb_parts--;
		}
	}

	cell = getCell_fromPos(end_pos_x, end_pos_y);
	if (cell) {
		cell->parts[cell->nb_parts] = part;
		cell->nb_parts = (cell->nb_parts+1) %MAX_PART_CELL;
	}
}
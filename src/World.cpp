#include "World.hpp"
#include <cmath>
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

World::~World() {
	if (grid) free(grid);
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


void World::giveCellPart(uint16_t x, uint16_t y, uint32_t part) {
	Cell& cell = getCell(x, y);
	cell.nb_parts = cell.nb_parts % MAX_PART_CELL;
	cell.parts[cell.nb_parts++] = part;
}

void World::giveCellSeg(uint16_t x, uint16_t y, uint32_t seg) {
	Cell& cell = getCell(x, y);
	cell.nb_segs = cell.nb_segs % MAX_SEG_CELL;
	cell.segs[cell.nb_segs++] = seg;
}


void World::giveCellPart(Cell* cell, uint32_t part) {
	cell->nb_parts = cell->nb_parts % MAX_PART_CELL;
	cell->parts[cell->nb_parts++] = part;
}

void World::giveCellSeg(Cell* cell, uint32_t seg) {
	cell->nb_segs = cell->nb_segs % MAX_SEG_CELL;
	cell->segs[cell->nb_segs++] = seg;
}


/**
* @details Empty each Cell then add each Particle to the right Cell.
* Another alternative would be to check for every Particle whether it belongs to a different Cell each time it moves.
* But I chose this implementation for now. 
*/
void World::update_grid_particle_contenance(Particle* particle_array, uint32_t array_size, float dt) {
	// std::cout << "World::update_grid_particle_contenance" << std::endl;
	grid_mutex.lock();
	// "Emptying" cells
	for (uint16_t y=0; y<gridSize[1]; y++) {
		for (uint16_t x=0; x<gridSize[0]; x++) {
			getCell(x, y).nb_parts = 0;
		}
	}

	// Filling them
	float next_pos[2];
	for (uint32_t i=0; i<array_size; i++) {
			next_pos[0] = particle_array[i].position[0] + particle_array[i].speed[0]*dt;
			next_pos[1] = particle_array[i].position[1] + particle_array[i].speed[1]*dt;

			if (0 <= next_pos[0] && next_pos[0] < size[0] &&
			    0 <= next_pos[1] && next_pos[1] < size[1]) {
				giveCellPart((uint16_t)(next_pos[0]/cellSize[0]), (uint16_t)(next_pos[1]/cellSize[1]), i);
			}
		// if (0 <= particle_array[i].position[0] && particle_array[i].position[0] < size[0] &&
		// 		0 <= particle_array[i].position[1] && particle_array[i].position[1] < size[1]) {
		// 	giveCellPart((uint16_t)(particle_array[i].position[0]/cellSize[0]), (uint16_t)(particle_array[i].position[1]/cellSize[1]), i);
		// }
	}
	grid_mutex.unlock();
}


void World::update_grid_segment_contenance(Segment* segment_array, uint32_t array_size) {
	// "Emptying" cells
	grid_mutex.lock();
	for (uint16_t y=0; y<gridSize[1]; y++) {
		for (uint16_t x=0; x<gridSize[0]; x++) {
			getCell(x, y).nb_segs = 0;
		}
	}

	// Filling them
	float vec[2];
	float start_pos[2];
	uint16_t start_coord[2], end_coord[2];
	float inc_length[2];
	float dx[2], dy[2];
	for (uint32_t i=0; i<array_size; i++) {
		vec[0] = segment_array[i].pos[1][0] - segment_array[i].pos[0][0];
		vec[1] = segment_array[i].pos[1][1] - segment_array[i].pos[0][1];

		if (vec[0] == 0) { // vertical line
			uint16_t min = segment_array[i].pos[0][1] < segment_array[i].pos[1][1] ? segment_array[i].pos[0][1] : segment_array[i].pos[1][1];
			uint16_t max = segment_array[i].pos[0][1] > segment_array[i].pos[1][1] ? segment_array[i].pos[0][1] : segment_array[i].pos[1][1];
			min /= cellSize[1];
			max /= cellSize[1];
			uint16_t x = segment_array[i].pos[0][0] / cellSize[0];
			uint16_t x_off;
			int8_t off_min = -1*(x!=0), off_max = 1+(x!=gridSize[0]-1);
			for (uint16_t y=min; y<=max; y++) {
				for (int8_t off=off_min; off<off_max; off++) {
					x_off = x+off;
					giveCellSeg(x_off, y, i);
				}
			}
		}
		else if (vec[1] == 0) { // horizontal line
			uint16_t min = segment_array[i].pos[0][0] < segment_array[i].pos[1][0] ? segment_array[i].pos[0][0] : segment_array[i].pos[1][0];
			uint16_t max = segment_array[i].pos[0][0] > segment_array[i].pos[1][0] ? segment_array[i].pos[0][0] : segment_array[i].pos[1][0];
			min /= cellSize[0];
			max /= cellSize[0];
			uint16_t y = segment_array[i].pos[0][1] / cellSize[1];
			uint16_t y_off;
			int8_t off_min = -1*(y!=0), off_max = 1+(y!=gridSize[1]-1);
			for (uint16_t x=min; x<=max; x++) {
				for (int8_t off=off_min; off<off_max; off++) {
					y_off = y+off;
					giveCellSeg(x, y_off, i);
				}
			}
		}
		else {
			end_coord[0] = segment_array[i].pos[1][0] / cellSize[0];
			end_coord[1] = segment_array[i].pos[1][1] / cellSize[1];
			// Adding end of the segment in the Cell
			giveCellSeg(end_coord[0], end_coord[1], i);

			float slope = vec[0] / vec[1]; // dx/dy
			bool x_positive = 0 < vec[0] ;
			bool y_positive = 0 < vec[1];
			bool x_moved = false;

			int8_t offset[2] = {static_cast<int8_t>(2*y_positive-1), static_cast<int8_t>(1-2*x_positive)};
			uint16_t z_off[2];

			start_pos[0] = segment_array[i].pos[0][0];
			start_pos[1] = segment_array[i].pos[0][1];
			start_coord[0] = start_pos[0] / cellSize[0];
			start_coord[1] = start_pos[1] / cellSize[1];

			// I need to initialize lengths so I don't have to do multiple checks x_moved ? & y_moved ?
			dx[0] = x_positive ? (start_coord[0]+1)*cellSize[0] - start_pos[0] : start_coord[0]*cellSize[0] - start_pos[0];
			dy[0] = dx[0] / slope;
			inc_length[0] = sqrt(dx[0]*dx[0] + dy[0]*dy[0]);
			dy[1] = y_positive ? (start_coord[1]+1)*cellSize[1] - start_pos[1] : start_coord[1]*cellSize[1] - start_pos[1];
			dx[1] = dy[1] * slope;
			inc_length[1] = sqrt(dx[1]*dx[1] + dy[1]*dy[1]);

			while (start_coord[0] != end_coord[0] || start_coord[1] != end_coord[1]) {
				// Adding this part of the segment in the Cell
				giveCellSeg(start_coord[0], start_coord[1], i);

				// adding cells next to the line to make it 3 cells wide
				z_off[0] = start_coord[0] + offset[0];
				z_off[1] = start_coord[1] + offset[1];
				if (z_off[0] < gridSize[0] && z_off[1] < gridSize[1]) {
					giveCellSeg(z_off[0], z_off[1], i);
				}
				z_off[0] = start_coord[0] - offset[0];
				z_off[1] = start_coord[1] - offset[1];
				if (z_off[0] < gridSize[0] && z_off[1] < gridSize[1]) {
					giveCellSeg(z_off[0], z_off[1], i);
				}

				if (x_moved) {
					dx[0] = x_positive ? (start_coord[0]+1)*cellSize[0] - start_pos[0] : start_coord[0]*cellSize[0] - start_pos[0];
					dy[0] = dx[0] / slope;
					inc_length[0] = sqrt(dx[0]*dx[0] + dy[0]*dy[0]);
				}
				else {
					dy[1] = y_positive ? (start_coord[1]+1)*cellSize[1] - start_pos[1] : start_coord[1]*cellSize[1] - start_pos[1];
					dx[1] = dy[1] * slope;
					inc_length[1] = sqrt(dx[1]*dx[1] + dy[1]*dy[1]);
				}

				if (inc_length[0] < inc_length[1]) {
					start_coord[0] += 2*x_positive-1;
					start_pos[0] += dx[0];
					start_pos[1] += dy[0];
					dx[1] -= dx[0];
					dy[1] -= dy[0];
					inc_length[1] -= inc_length[0];
					x_moved = true;
				} else {
					start_coord[1] += 2*y_positive-1;
					start_pos[0] += dx[1];
					start_pos[1] += dy[1];
					dx[0] -= dx[1];
					dy[0] -= dy[1];
					inc_length[0] -= inc_length[1];
					x_moved = false;
				}

			}

			// Adding the segment to the cells around the start and end coordinates as this is not taken care of in the while loop
			bool already_in;
			Cell* cell;
			uint16_t coord[2][2] = {
				end_coord[0], end_coord[1],
				static_cast<uint16_t>(segment_array[i].pos[0][0] / cellSize[0]), static_cast<uint16_t>(segment_array[i].pos[0][1] / cellSize[1])};
			for (uint8_t c=0; c<2; c++) {
				for (int8_t y_off=-1; y_off<2; y_off++) {
					for (int8_t x_off=-1; x_off<2; x_off++) {
						z_off[0] = coord[c][0] + x_off;
						z_off[1] = coord[c][1] + y_off;
						if (z_off[0] < gridSize[0] && z_off[1] < gridSize[1]) {
							already_in = false;
							cell = getCell_ptr(z_off[0], z_off[1]);
							for (uint8_t seg=0; seg<cell->nb_segs && !already_in; seg++) {
								already_in = i == cell->segs[seg];
							}
							if (!already_in) {
								giveCellSeg(cell, i);
							}
						}
					}
				}
			}
		}
	}
	grid_mutex.unlock();
}



void World::change_cell_part(uint32_t part, float init_pos_x, float init_pos_y, float end_pos_x, float end_pos_y) {
	Cell* cell = getCell_fromPos(init_pos_x, init_pos_y);
	if (cell) {
		// search the index of the Particle in the Cell
		uint8_t foundAt = -1;
		for (uint8_t i=0; i<cell->nb_parts; i++) {
			if (cell->parts[i] == part){
				foundAt = i;
				break;
			}
		}
		if (foundAt != (uint8_t)-1) { // If Particle is found in the Cell, remove it
			for (uint8_t i=foundAt+1; i<cell->nb_parts; i++) {
				cell->parts[i-1] = cell->parts[i];
			}
			cell->nb_parts--;
		}
	}

	// Adding the Particle in the end Cell
	cell = getCell_fromPos(end_pos_x, end_pos_y);
	if (cell) {
		giveCellPart(cell, part);
	}
}
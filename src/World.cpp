#include "World.hpp"
#include "utilities.hpp"

#include <cmath>
#include <cstring>
#include <iostream>

Cell Cell::nullCell;

WorldParam WorldParam::Default {
	Default.size[0] = 1800,
	Default.size[1] = 1000,
	Default.cellSize[0] = 4,
	Default.cellSize[1] = 4,
	
	Rectangle(10, 10, WorldParam::Default.size[0]-20, WorldParam::Default.size[1]-20), //spawn_rect
};

World::World(WorldParam& parameters) {
	params = parameters;
	std::cout << "World::World : Size [" << parameters.size[0] << ", " << parameters.size[1] << "]   cellSize [" << parameters.cellSize[0] << ", " << parameters.cellSize[1] << "]" << std::endl;
	gridSize[0] = ceil(params.size[0] / params.cellSize[0]);
	gridSize[1] = ceil(params.size[1] / params.cellSize[1]);
	grid = new Cell[gridSize[0]*gridSize[1]];
	std::cout << "\tgridSize :[" << gridSize[0] << ", " << gridSize[1] << "]" << " (" << i2s(gridSize[0]*gridSize[1]* sizeof(Cell)) << " bytes)" << std::endl;
	
	empty_grid(grid);
	
	// if (params.segments_in_grid) {
	// 	grid_seg = new Cell_seg[gridSize[0]*gridSize[1]];
	// 	empty_grid(grid_seg);
	// }
	// std::cout << "\tsegments_in_grid=" << b2s(params.segments_in_grid) << " => grid_seg (" << i2s( (params.segments_in_grid)*gridSize[0]*gridSize[1]* sizeof(Cell_seg) ) << " bytes)" << std::endl;
}

World::~World() {
	std::cout << "World::~World()" << std::endl;
	if (grid) delete[] grid;
	if (filled_coords) delete[] filled_coords;
	if (grid_seg) delete[] grid_seg;
}


Cell* World::getCell_fromPos(float pos_x, float pos_y) {
	if (0 <= pos_x && pos_x <= params.size[0] &&
	    0 <= pos_y && pos_y <= params.size[1]) {
		return &grid[(uint16_t)(pos_y/params.cellSize[1])*gridSize[0] + (uint16_t)(pos_x/params.cellSize[0])];
	}
	else return &Cell::nullCell;
}


bool World::getCellCoord_fromPos(float pos_x, float pos_y, uint16_t* x, uint16_t* y) {
	*x = (uint16_t)(pos_x/params.cellSize[0]);
	*y = (uint16_t)(pos_y/params.cellSize[1]);
	return (*x <= params.size[0] && *y <= params.size[1]); // using uint underflow to check for negative position.
}

void World::giveCellPart(uint16_t x, uint16_t y, uint32_t part) {
	giveCellPart(getCell(x, y), part);
}

void World::giveCellSeg(uint16_t x, uint16_t y, uint16_t seg) {
	// std::cout << "giveCellSeg(" << x << ", " << y << ", " << seg << ")" << std::endl;
	if (segments_in_grid) {
		Cell_seg& cell = getCell_seg(x, y);
		cell.segs[cell.nb_segs%MAX_SEG_CELL] = seg;
		cell.nb_segs += (cell.nb_segs < MAX_SEG_CELL);
	} else {
		seg_array[seg].cells.emplace_back(x, y);
	}

	n_cell_seg.fetch_add(1);
}

void World::remCellSeg(uint16_t x, uint16_t y, uint16_t seg) {
	Cell_seg& cell = getCell_seg(x, y);
	for (uint8_t i=0; i<cell.nb_segs; i++) { // searching for segment in cell
		if (cell.segs[i] == seg) { // found
			for (uint8_t j=i+1; j<cell.nb_segs; j++) {
				cell.segs[j-1] = cell.segs[j];
			}
			cell.nb_segs--;
			return;
		}
	}
	n_cell_seg.fetch_sub(1);
}


void World::giveCellPart(Cell& cell, uint32_t part) {
	// std::cout << "giveCellPart(" << cell << ", " << part << ")" << std::endl;
	uint8_t index = cell.nb_parts.load();
	cell.nb_parts.store(index + (index!=MAX_PART_CELL));
	cell.parts[index%MAX_PART_CELL] = part;
}

void World::giveCellSeg(Cell_seg* cell, uint16_t seg) {
	cell->segs[cell->nb_segs%MAX_SEG_CELL] = seg;
	cell->nb_segs += (cell->nb_segs < MAX_SEG_CELL);

	n_cell_seg.fetch_add(1);
}


bool World::will_use_nParticles(uint32_t max_n) {
	if (2*max_n < gridSize[0]*gridSize[1]) { // Then we consider emptying the grid blindly is less efficient than emptying the cell we know are filled
		// This is basically trading memory for speed.
		filled_coords = new uint16_t[max_n*2];
		memset(filled_coords, 0, max_n*2*sizeof(uint16_t));
		empty_blind = false;
	} else {
		empty_blind = true;
		if (filled_coords) {
			delete[] filled_coords;
			filled_coords = nullptr;
		}
	}

	chg_seg_store_sys(max_n);

	std::cout << "\tempty_blind=" << b2s(empty_blind) << " => filled_coords (" << i2s((!empty_blind)*max_n*2*sizeof(uint16_t) ) << " bytes)" << std::endl;
	return empty_blind;
}


void World::empty_grid_particle_blind(uint32_t start, uint32_t end) {
	for (uint16_t y=start; y<end; y++) {
		for (uint16_t x=0; x<gridSize[0]; x++) {
			getCell(x, y).nb_parts = 0;
		}
	}
}

void World::empty_grid_particle_pbased(uint32_t p_start, uint32_t p_end) {
	// std::cout << "World::empty_grid_particle_pbased(" << p_start << ", " << p_end << ")" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		Cell& cell = getCell(filled_coords[2*p], filled_coords[2*p+1]);
		cell.nb_parts = 0;
	}
}

template<typename T>
void World::empty_grid(T* grille) {
	memset(grille, 0, gridSize[0]*gridSize[1]*sizeof(T)); // It is important to set to 0 padding bits for bit-wise comparisons
}

/**
* @details Adding each Particle in the Cell it would be at the next iteration if nothing changes its speed.
* This seems to make the simulation more stable.
* But it comes at the cost of having to remember each Particle position as the speed will probably be changed by next iteration.
*/
void World::update_grid_particle_contenance(Particle* particle_array, uint32_t p_start, uint32_t p_end, float dt) {
	// std::cout << "World::update_grid_particle_contenance(" << particle_array << ", " << p_start << ", " << p_end << ", " << dt << ")" << std::endl;
	uint16_t next_pos[2];
	for (uint32_t i=p_start; i<p_end; i++) {
			next_pos[0] = (particle_array[i].position[0] + particle_array[i].speed[0]*dt) / params.cellSize[0];
			next_pos[1] = (particle_array[i].position[1] + particle_array[i].speed[1]*dt) / params.cellSize[1];

			if (next_pos[0] < gridSize[0] && next_pos[1] < gridSize[1]) {
				giveCellPart(next_pos[0], next_pos[1], i);
				if (!empty_blind) {
					filled_coords[2*i   ] = next_pos[0];
					filled_coords[2*i +1] = next_pos[1];
				}
			}
	}
}


void World::go_through_segment(uint16_t seg, void(World::*fun_over_cell)(uint16_t, uint16_t, uint16_t)) {
	// std::cout << "go_through_segment " << seg << std::endl;
	float Ax = seg_array[seg].pos[0][0];
	float Ay = seg_array[seg].pos[0][1];
	float Bx = seg_array[seg].pos[1][0];
	float By = seg_array[seg].pos[1][1];

	float vec[2] = {Bx - Ax, By - Ay};
	float start_pos[2];
	uint16_t start_coord[2], end_coord[2];
	float inc_length[2];
	float dx[2], dy[2];

	if (vec[0] == 0 || vec[1] == 0) { // If the Segment is either vertical or horizontal
		bool isVertical = vec[0] == 0;

		uint16_t min = seg_array[seg].min[isVertical] / params.cellSize[isVertical] - 0.5;
		min = min < gridSize[isVertical] ? min : 0;
		uint16_t max = seg_array[seg].max[isVertical] / params.cellSize[isVertical] + 0.5;
		max = max < gridSize[isVertical] ? max : gridSize[isVertical]-1;

		uint16_t xy[2];
		xy[!isVertical] = (Ax*isVertical + Ay*!isVertical) / params.cellSize[!isVertical]; // One coordinate is constant. The !isVertical one.
		int8_t off_min = -1*(xy[!isVertical]!=0);
		int8_t off_max = 1+(xy[!isVertical]!=gridSize[!isVertical]-1); // Offset to give the line a width in the Cells (without going out of the grid)
		for (xy[isVertical]=min; xy[isVertical]<=max; xy[isVertical]++) {
			for (int8_t off=off_min; off<off_max; off++) {
				(this->*fun_over_cell)(xy[0]+off*isVertical, xy[1]+off*!isVertical, seg);
			}
		}

	}
	else {
		float slope = vec[0] / vec[1]; // dx/dy
		bool x_positive = 0 < vec[0] ;
		bool y_positive = 0 < vec[1];
		bool x_moved = false;

		int8_t offset[2] = {static_cast<int8_t>(2*y_positive-1), static_cast<int8_t>(1-2*x_positive)};
		uint16_t z_off[2];

		start_pos[0] = Ax;
		start_pos[1] = Ay;
		end_coord[0] = Bx / params.cellSize[0];
		end_coord[1] = By / params.cellSize[1];
		start_coord[0] = start_pos[0] / params.cellSize[0];
		start_coord[1] = start_pos[1] / params.cellSize[1];

		// I need to initialize lengths so I don't have to do multiple checks x_moved ? & y_moved ?
		dx[0] = x_positive ? (start_coord[0]+1)*params.cellSize[0] - start_pos[0] : start_coord[0]*params.cellSize[0] - start_pos[0];
		dy[0] = dx[0] / slope;
		inc_length[0] = sqrt(dx[0]*dx[0] + dy[0]*dy[0]);
		dy[1] = y_positive ? (start_coord[1]+1)*params.cellSize[1] - start_pos[1] : start_coord[1]*params.cellSize[1] - start_pos[1];
		dx[1] = dy[1] * slope;
		inc_length[1] = sqrt(dx[1]*dx[1] + dy[1]*dy[1]);

		// Testing for traveled length along the segment because the simple check start_coord==end_cord breaks when the end of the segment is at the corner of 4 cells
		float total_length = sqrt((Bx-Ax)*(Bx-Ax) + (By-Ay)*(By-Ay));
		float cumulated_length = 0;

		while ((start_coord[0] != end_coord[0] || start_coord[1] != end_coord[1]) && cumulated_length < total_length) {
			// Adding this part of the segment in the Cell
			(this->*fun_over_cell)(start_coord[0], start_coord[1], seg);
			// std::this_thread::sleep_for(std::chrono::milliseconds(10));

			// adding cells next to the line to make it 3 cells wide
			z_off[0] = start_coord[0] + offset[0];
			z_off[1] = start_coord[1] + offset[1];
			if (z_off[0] < gridSize[0] && z_off[1] < gridSize[1]) {
				(this->*fun_over_cell)(z_off[0], z_off[1], seg);
			}
			z_off[0] = start_coord[0] - offset[0];
			z_off[1] = start_coord[1] - offset[1];
			if (z_off[0] < gridSize[0] && z_off[1] < gridSize[1]) {
				(this->*fun_over_cell)(z_off[0], z_off[1], seg);
			}

			// Calculating next Cell
			if (x_moved) {
				dx[0] = x_positive ? (start_coord[0]+1)*params.cellSize[0] - start_pos[0] : start_coord[0]*params.cellSize[0] - start_pos[0];
				dy[0] = dx[0] / slope;
				inc_length[0] = sqrt(dx[0]*dx[0] + dy[0]*dy[0]);
			}
			else {
				dy[1] = y_positive ? (start_coord[1]+1)*params.cellSize[1] - start_pos[1] : start_coord[1]*params.cellSize[1] - start_pos[1];
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
				cumulated_length += inc_length[0];
				x_moved = true;
			} else {
				start_coord[1] += 2*y_positive-1;
				start_pos[0] += dx[1];
				start_pos[1] += dy[1];
				dx[0] -= dx[1];
				dy[0] -= dy[1];
				inc_length[0] -= inc_length[1];
				cumulated_length += inc_length[1];
				x_moved = false;
			}

		}

		// Going through the segment to the cells around the start and end coordinates as this is not taken care of in the while loop
		bool already_in;
		uint16_t coord[2][2] = {
			static_cast<uint16_t>(Ax / params.cellSize[0]), static_cast<uint16_t>(Ay / params.cellSize[1]),
			end_coord[0], end_coord[1]
		};
		for (uint8_t c=0; c<2; c++) {
			for (int8_t y_off=-(bool)coord[c][1]; y_off<2; y_off++) {
				for (int8_t x_off=-(bool)coord[c][0]; x_off<2; x_off++) {

					z_off[0] = coord[c][0] + x_off;
					z_off[1] = coord[c][1] + y_off;
					if (z_off[0] < gridSize[0] && z_off[1] < gridSize[1]) {

						already_in = false;
						if (segments_in_grid) {
							Cell_seg& cell = getCell_seg(z_off[0], z_off[1]);
							for (uint8_t i=0; i<cell.nb_segs && !already_in; i++) {
								already_in = seg == cell.segs[i];
							}
						}
						else {
							std::vector<GridCoord>& cells = seg_array[seg].cells;
							for (uint8_t i=0; i<std::min((unsigned long)10, cells.size()) && !already_in; i++) {
								uint16_t index = (!c)*i + c*(cells.size()-1-i);
								index = index <= cells.size()-1 ? index : cells.size()-1; // happens if cells.size() < 9
								already_in = cells[index][0] == z_off[0] && cells[index][1] == z_off[1];
							}
						}
						if (!already_in) {
							(this->*fun_over_cell)(z_off[0], z_off[1], seg);
						}

					}
				}
			}
		}

	}
}


void World::add_segment(float Ax, float Ay, float Bx, float By) {
	// std::cout << "Add (" << Ax << ", " << Ay << "), (" << Bx << ", " << By << "), " << std::endl;
	float pos[2][2] = {Ax, Ay, Bx, By};
	
	// Correcting Segment coordinates so that it is entirely in the World. If the segment is entirely outside the world, it is not created.
	bool c=0; // coordinate on which we are working
	do {
		if (pos[0][c] == pos[1][c]) { // vertical or horizontal
			if (pos[0][c] < 0 || params.size[c] < pos[0][c]) return; // segment entirely outside the world; like -- [  ]
			pos[0][!c] = std::max(pos[0][!c], 0.f);
			pos[0][!c] = std::min(pos[0][!c], params.size[!c]);
			pos[1][!c] = std::max(pos[1][!c], 0.f);
			pos[1][!c] = std::min(pos[1][!c], params.size[!c]);
			if (pos[0][!c] == pos[1][!c]) return; // Segment is entirely outside of the world; like [ __ ]
																						// This also removes point-like segments
			// break;
		}
		else { // Neither
			bool inversion = pos[1][c] < pos[0][c];
			if (inversion) { // making so the segment is going from along increasing axis c
				std::swap(pos[0][0], pos[1][0]);
				std::swap(pos[0][1], pos[1][1]);
			}
			if (pos[1][c] <= 0 || params.size[c] <= pos[0][c]) return; // -- |  |  OR |  | --
			if (pos[0][c] < 0 && 0 < pos[1][c]) { // -|- |
				// cap A
				pos[0][!c] = pos[1][!c] + (pos[0][!c]-pos[1][!c])/(pos[0][c]-pos[1][c]) * (0-pos[1][c]);
				pos[0][c] = 0;
			}
			if (pos[0][c] < params.size[c] && params.size[c] < pos[1][c]) { // | -|-
				// cap B
				pos[1][!c] = pos[0][!c] + (pos[1][!c]-pos[0][!c])/(pos[1][c]-pos[0][c]) * (params.size[c]-pos[0][c]);
				pos[1][c] = params.size[c];
			}
			if (inversion) { // exchanging A and B back
				std::swap(pos[0][c], pos[1][c]);
				std::swap(pos[0][!c], pos[1][!c]);
			}
		}
		c = !c;
	} while (c);

	// std::cout << "\tSegment " << seg_array.size() << " will be added (" << pos[0][0] << ", " << pos[0][1] << "), (" << pos[1][0] << ", " << pos[1][1] << ")" << std::endl;

	grid_seg_mutex.lock();
	seg_array.emplace_back(pos[0][0], pos[0][1], pos[1][0], pos[1][1]);
	go_through_segment(seg_array.size()-1, &World::giveCellSeg);
	grid_seg_mutex.unlock();
}

void World::rem_segment(uint16_t index) {
	if (seg_array.size()) {
		grid_seg_mutex.lock();
		if (segments_in_grid) {
			go_through_segment(index, &World::remCellSeg);
		} else n_cell_seg.fetch_sub(seg_array[index].cells.size());

		seg_array.erase(seg_array.begin() + index);
		grid_seg_mutex.unlock();
	}
}

void World::chg_seg_store_sys(uint32_t nb_parts) {
	uint8_t i=0;
	if ((segments_in_grid && (n_cell_seg.load() < nb_parts)) ||
		 (!segments_in_grid && (1.2f* nb_parts < n_cell_seg.load())))
		{
		
			std::cout << "Change in Segment storing system : number of Particles = " << nb_parts << ",  while number of Cells with a segment = " << n_cell_seg.load() << std::endl;
			
			grid_seg_mutex.lock();
			segments_in_grid = !segments_in_grid;
			if (segments_in_grid) { // segment storage -> grid storage
		
				grid_seg = new Cell_seg[gridSize[0]*gridSize[1]];
				empty_grid(grid_seg);
		
				for (uint16_t s=0; s<seg_array.size(); s++) {
					for (auto& coord : seg_array[s].cells) {
						giveCellSeg(coord[0], coord[1], s);
					}
					n_cell_seg.fetch_sub(seg_array[s].cells.size()); // because giveCellSeg increments n_cell_seg
					seg_array[s].cells.resize(0);
					seg_array[s].cells.shrink_to_fit(); // If change happens often this line is inefficient.
				}
			}
			else { // grid storage -> segment storage
				if (grid_seg) delete[] grid_seg;
				grid_seg = nullptr;
				for (uint16_t s=0; s<seg_array.size(); s++) {
					go_through_segment(s, &World::giveCellSeg);
					n_cell_seg.fetch_sub(seg_array[s].cells.size()); // because giveCellSeg increments n_cell_seg
				}
			}
			grid_seg_mutex.unlock();
	}
}

void World::add_zone(int8_t function, float posX, float posY, float sizeX, float sizeY) {
	float rect[2][2] = {posX, posY, sizeX+posX, sizeY+posY}; // absolute coordinates, size is also changed if capping coordinates
	GridCoord gridCoord[2];
	for (uint8_t c=0; c<2; c++) {
		for (uint8_t i=0; i<2; i++) {
			if (rect[i][c]<0) {
				rect[i][c]=0;
				gridCoord[i].coord[c] = 0;
			}
			else if (params.size[c] <= rect[i][c]) {
				rect[i][c] = params.size[c];
				gridCoord[i].coord[c] = gridSize[c];
			}
			else {
				gridCoord[i].coord[c] = std::min( (uint16_t)std::round(rect[i][c]/params.cellSize[c]), gridSize[c] );
				rect[i][c] = gridCoord[i].coord[c] * params.cellSize[c];
			}
		}
		if (rect[0][c] == rect[1][c]) return; // don't add the zone if its width is null
		else if (rect[1][c] < rect[0][c]) {
			std::swap(rect[0][c], rect[1][c]);
			std::swap(gridCoord[0].coord[c], gridCoord[1].coord[c]);
		}
		rect[1][c] = rect[1][c] - rect[0][c];
	}
	zone_covered_cells += (gridCoord[1].coord[0] - gridCoord[0].coord[0]) * (gridCoord[1].coord[1] - gridCoord[0].coord[1]);
	zones.emplace_back(function, rect, gridCoord);
}


void World::change_cell_part(uint32_t part, float init_pos_x, float init_pos_y, float end_pos_x, float end_pos_y) {
	// std::cout << "change cell part" << std::endl;
	Cell* cell_init = getCell_fromPos(init_pos_x, init_pos_y);
	Cell* cell_end  = getCell_fromPos(end_pos_x,  end_pos_y);
	if (cell_init == cell_end) return; // if the start cell and the end cell are the same, we don't need to move the Particle from cell to cell.

	// Removing the Particle from the start Cell
	if (cell_init->isReal()) {
		// search the index of the Particle in the Cell
		uint8_t foundAt = -1;
		for (uint8_t i=0; i<cell_init->nb_parts.load(); i++) {
			if (cell_init->parts[i] == part){
				foundAt = i;
				break;
			}
		}
		if (foundAt != (uint8_t)-1) { // If Particle is found in the Cell, remove it
			for (uint8_t i=foundAt+1; i<cell_init->nb_parts.load(); i++) {
				cell_init->parts[i-1] = cell_init->parts[i];
			}
			cell_init->nb_parts.fetch_sub(1);
			// cell_init->nb_parts--;
		}
	}

	// Adding the Particle in the end Cell
	if (cell_end->isReal()) {
		giveCellPart(*cell_end, part);
		if (!empty_blind) {
			filled_coords[2*part   ] = end_pos_x / params.cellSize[0];
			filled_coords[2*part +1] = end_pos_y / params.cellSize[1];
		}
	}
}

// I need to remake this function So that it can handle the new segment grid system
void World::verify_grid_contenance(uint32_t max_to_be_found, uint32_t p_array_size, uint32_t s_array_size) {
	/*
	uint32_t found = 0;
	for (uint16_t y=0; y<gridSize[1]; y++) {
		for (uint16_t x=0; x<gridSize[0]; x++) {
			Cell& cell = getCell(x, y);
			uint8_t nbp = cell.nb_parts.load();
			found += nbp;
			if (nbp > MAX_PART_CELL || cell.nb_segs > MAX_SEG_CELL) { // Verifying that there aren't too many particles or segments in that cell
				std::cout << "Cell overloading " << x << ", " << y << "  :  parts=" << (short)nbp << ",  nb_segs=" << (short)cell.nb_segs << std::endl;
			}
			else {
				for (uint8_t i=0; i<nbp; i++) { // Verifying that each Particle contained is in the range of indexes
					uint32_t parti = cell.parts[i]; // \o/
					if (p_array_size <= parti) {
						std::cout << "Corruption of Cell's data " << x << ", " << y << "part[" << (short)i << "] = " << parti << std::endl;
					}
				}
				for (uint8_t i=0; i<cell.nb_segs; i++) { // Verifying that each Segment contained is in the range of indexes
					uint32_t segsi = cell.segs[i]; // \o/2
					if (s_array_size <= segsi) {
						std::cout << "Corruption of Cell's data " << x << ", " << y << "seg[" << (short)i << "] = " << segsi << std::endl;
					}
				}
			}
		}
	}
	if (found > max_to_be_found) {
		std::cout << "More Particles in the grid than given" << std::endl;
	}
	*/
}
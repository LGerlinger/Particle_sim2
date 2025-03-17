#pragma once

#include "Particle.hpp"
#include "Segment.hpp"

#include <cstdint>

#define MAX_PART_CELL 4
#define MAX_SEG_CELL 4

/**
* The world is divided in cells for better performance.
* Each Cell can contain MAX_PART_CELL particles and MAX_SEG_CELL segments
*/
struct Cell {
	uint8_t nb_parts;
	uint8_t nb_segs;
	uint32_t parts[MAX_PART_CELL];
	uint32_t segs[MAX_SEG_CELL];
};

class World {
private :
	float size[2];

	uint16_t gridSize[2];
	float cellSize[2];
	
	Cell* grid = nullptr;

public:
	World(float sizeX, float sizeY, float cellSizeX, float cellSizeY);
	~World();

	inline float getSize(bool xy) const {return size[xy];};
	inline uint16_t getGridSize(bool xy) const {return gridSize[xy];};
	inline float getCellSize(bool xy) const {return cellSize[xy];};
	
	inline Cell& getCell(uint16_t x, uint16_t y) {return grid[y*gridSize[0] +x];};

	/**
	* @brief Returns the cell where the point [pos_x, pos_y] is.
	* @return If [pos_x, pos_y] is whithin the world borders, returns the cell pointer, else returns nullpointer.
	*/
	Cell* getCell_fromPos(float pos_x, float pos_y);

	/**
	* @brief Changes [x, y] to the corresponding cell coordinates where the point [pos_x, pos_y] is .
	* @return Returns whether [pos_x, pos_y] is whithin the world borders.
	*/
	bool getCellCoord_fromPos(float pos_x, float pos_y, uint16_t* x, uint16_t* y);

	/**
	* @brief Put the Particles (whithin the world borders) in their corresponding Cells.
	* This updates the parts attribute of the Cells of grid.
	*/
	void update_grid_particle_contenance(Particle* particle_array, uint32_t array_size);

	/**
	* @brief Put the Segments in their corresponding Cells.
	* This updates the segs attribute of the Cells of grid.
	*/
	void update_grid_segment_contenance(Segment* segment_array, uint32_t array_size);

	/**
	* @brief Remove a Particle from a Cell to add it to another.
	* @param part is the Particle's number in the array containing all Particles. 
	*/
	void change_cell_part(uint32_t part, float init_pos_x, float init_pos_y, float end_pos_x, float end_pos_y);
};
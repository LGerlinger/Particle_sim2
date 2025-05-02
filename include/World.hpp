#pragma once

#include "utilities.hpp"
#include "Particle.hpp"
#include "Segment.hpp"

#include <atomic>
#include <cstdint>


#define MAX_PART_CELL 4
#define MAX_SEG_CELL 2

/**
* The world is divided in cells for better performance.
* Each Cell can contain MAX_PART_CELL particles and MAX_SEG_CELL segments
*/
struct Cell {
	std::atomic_uint8_t nb_parts;
	uint8_t nb_segs;
	uint32_t parts[MAX_PART_CELL];
	uint16_t segs[MAX_SEG_CELL];

	static Cell nullCell;
	bool isReal() {return this != &nullCell;}
};

class World {
private :
	float size[2];

	uint16_t gridSize[2];
	float cellSize[2];
	bool empty_blind = true; //< Whether the world should be emptied blindly or by using filled_coords.
	
	Cell* grid = nullptr;
	uint16_t* filled_coords = nullptr; //< List of grid coordinates where a Particle has been added to the grid. Used like part.x=[2*part_index], part.y=[2*part_index +1]

	inline void giveCellPart(uint16_t x, uint16_t y, uint32_t part);
	inline void giveCellSeg(uint16_t x, uint16_t y, uint16_t seg);
	inline void giveCellPart(Cell* cell, uint32_t part);
	inline void giveCellSeg(Cell* cell, uint16_t seg);

public:
	World(float sizeX, float sizeY, float cellSizeX, float cellSizeY, uint32_t max_particle_used, bool force_empty_pbased = false);
	~World();

	inline float getSize(bool xy) const {return size[xy];};
	inline uint16_t getGridSize(bool xy) const {return gridSize[xy];};
	inline float getCellSize(bool xy) const {return cellSize[xy];};
	
	inline Cell& getCell(uint16_t x, uint16_t y) {return grid[y*gridSize[0] +x];};
	inline Cell* getCell_ptr(uint16_t x, uint16_t y) {return &grid[y*gridSize[0] +x];};

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
	* @brief Puts the Particles (whithin the world borders) in their corresponding Cells.
	* This updates the parts attribute of the Cells of grid.
	*/
	void update_grid_particle_contenance(Particle* particle_array, uint32_t p_start, uint32_t p_end, float dt);

	/**
	* @brief Call this function before call empty_grid_particle_blind/pbased to know whether the world needs to be emptied blindly or using the particles position.
	* @warning Calling empty_grid_particle_pbased when the world is not prepared will write on an unallocated array (and cause a segmentation fault).
	*/
	inline bool emptying_blindly() {return empty_blind;};

	/**
	* @brief Puts the number of Particle at 0 in the cell in the lines in [start, end[.
	* @param start The first line to empty (included).
	* @param end The last line to empty (excluded).
	*/
	void empty_grid_particle_blind(uint32_t start, uint32_t end);
	/**
	* @brief Puts the number of Particles at 0 in cells where a Particle was added during update_grid_particle_contenance.
	* @details Doing can be much more efficient when the Cells outnumber the Particles to some extent.
	* @param p_start The first Particle which trace should be removed (included).
	* @param p_end The larst Particle which trace should be removed (excluded).
	*/
	void empty_grid_particle_pbased(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Empties every cell in the lines in [start, end[ of everything.
	* @details Really this function doesn't discriminate.
	* @param start The first line to empty (included).
	* @param end The last line to empty (excluded).
	*/
	void empty_grid(uint32_t start, uint32_t end);

	/**
	* @brief Puts the Segments in their corresponding Cells.
	* This updates the segs attribute of the Cells of grid.
	*/
	void update_grid_segment_contenance(Segment* segment_array, uint16_t array_size);

	/**
	* @brief Remove a Particle from a Cell to add it to another.
	* @param part is the Particle's number in the array containing all Particles. 
	*/
	void change_cell_part(uint32_t part, float init_pos_x, float init_pos_y, float end_pos_x, float end_pos_y);

	/**
	* @brief Checks for each Cell if its number of Particles or Segments is not beyound what can be stored.
	* Also checks if the Particle and Segment indices found in the Cells are correct (i.e. not over the arrays size).
	*/
	void verify_grid_contenance(uint32_t max_to_be_found, uint32_t p_array_size, uint32_t s_array_size);
};
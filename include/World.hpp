#pragma once

#include "Particle.hpp"
#include "Segment.hpp"
#include "Zone.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>


#define MAX_PART_CELL 4
#define MAX_SEG_CELL 2

/**
* The world is divided in cells for better performance.
* Each Cell can contain MAX_PART_CELL particles.
*/
struct Cell {
	std::atomic_uint8_t nb_parts;
	uint32_t parts[MAX_PART_CELL];

	static Cell nullCell;
	bool isReal() {return this != &nullCell;}
};

/**
* Sometimes it is better to have the grid store segment positions so I can iterate through the Particles for collisions.
* Other times, it is better to have the segments store their cell positions so I can iterate through those instead.
* It depends on the number of Particles, the combined size of the Segments and the number of Particles in contact with the segments.
*
* So I am creating 2 parallel grids, one for Particles, one for the Segments.
* Depending on the relative size of segments and number of particles I will either iterate through Particles or Segments for collisions. 
*/
struct Cell_seg {
	uint8_t nb_segs;
	uint16_t segs[MAX_SEG_CELL];

	static Cell_seg nullCell;
	bool isReal() {return this != &nullCell;}
};

/**
* Parameters describing a World.
*/
class WorldParam {
public :
	float size[2];
	float cellSize[2];
	Rectangle spawn_rect;

	static WorldParam Default;
};


class World {
private :
	WorldParam params;
	bool segments_in_grid = false;

	uint16_t gridSize[2];
	bool empty_blind = true; //< Whether the world should be emptied blindly or by using filled_coords.

	Cell* grid = nullptr;
	uint16_t* filled_coords = nullptr; //< List of grid coordinates where a Particle has been added to the grid. Used like part.x=[2*part_index], part.y=[2*part_index +1]
	Cell_seg* grid_seg = nullptr; //< A grid for Segments, i.e. each Cell of this grid knows wether a Segment is going though it.
	std::mutex grid_seg_mutex; //< A mutex locked before adding / removing / changing segments.

	inline void giveCellPart(Cell& cell, uint32_t part);
	inline void giveCellPart(uint16_t x, uint16_t y, uint32_t part);
	inline void giveCellSeg(uint16_t x, uint16_t y, uint16_t seg);
	        void remCellSeg(uint16_t x, uint16_t y, uint16_t seg);
	inline void giveCellSeg(Cell_seg* cell, uint16_t seg);

	std::vector<Zone> zones;
	uint64_t zone_covered_cells = 0; //< The number of Cells in the grid that are covered by a Zone.

public:
	std::atomic_uint64_t n_cell_seg = 0; //< Number of Cells with a Segment.
	std::vector<Segment> seg_array; //< Vector containing all the Segments.
	inline bool sig() {return segments_in_grid;};

	World(WorldParam& parameters = WorldParam::Default);
	// World(float sizeX, float sizeY, float cellSizeX, float cellSizeY, bool seg_in_grid);
	~World();

	inline WorldParam& getParams() {return params;};

	inline float getSize(bool xy) const {return params.size[xy];};
	inline uint16_t getGridSize(bool xy) const {return gridSize[xy];};
	inline float getCellSize(bool xy) const {return params.cellSize[xy];};
	
	inline Cell& getCell(uint16_t x, uint16_t y) {return grid[y*gridSize[0] +x];};
	inline Cell* getCell_ptr(uint16_t x, uint16_t y) {return &grid[y*gridSize[0] +x];};
	inline Cell_seg& getCell_seg(uint16_t x, uint16_t y) {return grid_seg[y*gridSize[0] +x];};
	inline Cell_seg* getCell_ptr_seg(uint16_t x, uint16_t y) {return &grid_seg[y*gridSize[0] +x];};

	/**
	* @brief Returns the cell where the point [pos_x, pos_y] is.
	* @return If [pos_x, pos_y] is whithin the world borders, returns the cell pointer, else returns nullpointer.
	*/
	Cell* getCell_fromPos(float pos_x, float pos_y);

	/**
	* @brief Changes [x, y] to the corresponding cell coordinates where the point [pos_x, pos_y] is.
	* @return Returns whether [pos_x, pos_y] is whithin the world borders.
	* @warning x and y are set even if they are out of the world borders (i.e. even if the function returns false).
	*/
	bool getCellCoord_fromPos(float pos_x, float pos_y, uint16_t* x, uint16_t* y);

	inline Rectangle& getSpawnRect() {return params.spawn_rect;};
	/**
	* @brief Adds a Zone in @see zones.
	* @details The coordinates of the new Zone are set to their closest Cell borders. The Zone always covers entirely the Cells it intersects with.
	* If this leads to the Zone having a width of 0, no Zone is created.
	* @param function The function number of the new Zone.
	* @param posX X coordinate of the top left of the new Zone. If it is negative, it will be set to 0.
	* @param posY Y coordinate of the top left of the new Zone. If it is negative, it will be set to 0.
	* @param posX Size along X axis of the new Zone.
	* @param posY Size along Y axis of the new Zone.
	*/
	void add_zone(int8_t function, float posX, float posY, float sizeX, float sizeY);
	inline uint16_t getNbOfZones() {return zones.size();};
	inline uint64_t getNZoneCoveredCells() {return zone_covered_cells;};
	inline Zone& getZone(uint16_t z) {return zones[z];};

	/**
	* @brief Puts the Particles (whithin the world borders) in their corresponding Cells.
	* This updates the parts attribute of the Cells of grid.
	*/
	void update_grid_particle_contenance(Particle* particle_array, uint32_t p_start, uint32_t p_end, float dt);

	bool will_use_nParticles(uint32_t max_n);

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
	* @brief Empties every cell in the grid "grille".
	* @details Really this function doesn't discriminate.
	* @param grille Grid to empty.
	*/
	template<typename T>
	void empty_grid(T* grille);

	/**
	* @brief A function to apply fun_over_cell over every Cell the Segment seg traverses.
	* This function uses a 3-cells width.
	*/
	void go_through_segment(uint16_t seg, void(World::*fun_over_cell)(uint16_t, uint16_t, uint16_t));

	/**
	* @brief Creates a Segment and gives it its Cells.
	* Either the Cells are stored in the segment or in the grid_seg
	*/
	void add_segment(float Ax, float Ay, float Bx, float By);
	inline void add_segment(float ends[4]) {add_segment(ends[0], ends[1], ends[2], ends[3]);};

	/**
	* @brief Deletes a Segment and remove it from its Cells.
	* Either the Cells are stored in the segment or in the grid_seg
	*/
	void rem_segment(uint16_t index);

	/**
	* @brief Changes the Segment-Cell storing system from grid-based to Segment-based and the other way around if needed.
	* Uses a hysteresis to check which system currently used is the best. 
	* @param nb_parts The number of Particles currently used in the simulation (for collisions with Segments). 
	*/
	void chg_seg_store_sys(uint32_t nb_parts=0);


	inline bool gsm_trylock() {return grid_seg_mutex.try_lock();};
	inline void gsm_unlock() {grid_seg_mutex.unlock();};

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
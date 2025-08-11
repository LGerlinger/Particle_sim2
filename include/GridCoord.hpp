#pragma once

#include <cstdint>

/**
* A coordinate in the World's grid.
*/
struct GridCoord {
	uint16_t coord[2];

	GridCoord() = default;
	GridCoord(uint16_t x, uint16_t y) : coord{x, y} {};
	// GridCoord(GridCoord& gc) : coord{gc[0], gc[1]} {};
	inline uint16_t operator[](bool b) {return coord[b];};
};

// typedef std::array<uint16_t, 2> GridCoord;
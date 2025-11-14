#version 460 core
layout (location = 0) in uint data; // Each Cell has 4 bytes of data (the minimum I can send), 2 are used.

out vec4 vertexColor;

uniform mat4 modelViewMat; // All the projection matrices.
uniform vec2 coord0; // World coordinate of the center of the top-left cell. Used to calculate the world coordinates of the current cell-vertex.
uniform float cellSize; // World cell size. Used to calculate the world coordinates of the current cell-vertex.
uniform uint cellsPerRows; // Used to calculate the position of the cell in the grid, and thus the world coordinates of the current cell-vertex.

uniform uint maxPartPerCell; // Maximum value of cell's 4th byte
uniform uint maxSegPerCell;  // Maximum value of cell's 3rd byte

uniform vec4 error_color;
uniform vec4 saturation_color;

void main()
{
	uint row = uint(floor(uint(gl_VertexID) / cellsPerRows));
	// uint col = uint(gl_VertexID) - cellsPerRows * row;
	vec2 world_pos = coord0 + vec2((uint(gl_VertexID) - cellsPerRows * row) * cellSize, row * cellSize);
	gl_Position = modelViewMat * vec4(world_pos, 0.0, 1.0);

	uint nbParticles = data & uint(0xFF);
	uint nbSegments = (data & uint(0xFF00)) >> 8;

	if (nbParticles < maxPartPerCell && nbSegments < maxSegPerCell) {
		vertexColor = vec4(0.0, float(nbParticles) / maxPartPerCell, float(nbSegments) / maxSegPerCell, 1.0);
	}
	else if (nbParticles > maxPartPerCell || nbSegments > maxSegPerCell) {
		vertexColor = error_color;
	}
	else {
		vertexColor = saturation_color;
	}
}

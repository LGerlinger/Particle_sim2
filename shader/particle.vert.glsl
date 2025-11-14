#version 460 core
// Cells are displayed in 2d
layout (location = 0) in vec2 pos; // Particle-vertex world position.
layout (location = 1) in vec2 speed; // Particle-vertex world speed.

out vec4 vertexColor;

uniform mat4 modelViewMat; // All the projection matrices.
uniform float max_speed; // Maximum speed of a Particle (to normalise the speed).

void main()
{
	gl_Position = modelViewMat * vec4(pos, 0.0, 1.0);
	float speed_ratio = length(speed) / max_speed;
	vertexColor.r = speed_ratio;
	vertexColor.g = speed_ratio / 4;
	vertexColor.b = speed_ratio / 8;
	vertexColor.a = 1.0;
}
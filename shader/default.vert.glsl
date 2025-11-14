#version 460 core
layout (location = 0) in vec2 pos; // Each Segment has 2 pos of start.

uniform mat4 modelViewMat; // All the projection matrices.

void main()
{
	gl_Position = modelViewMat * vec4(pos, 0.0, 1.0);
}

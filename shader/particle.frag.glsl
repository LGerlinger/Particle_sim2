#version 460 core
in vec4 vertexColor;

out vec4 FragColor;

void main()
{
	// vec2 insidePos = gl_PointCoord*2 -1; // transform coordinates to [-1;1]
	// float dist = length(insidePos);
	FragColor = vertexColor;
	FragColor.a = FragColor.a * (1-smoothstep(0.8, 1, length(gl_PointCoord*2 -1)));
}
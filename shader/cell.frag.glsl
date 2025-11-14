#version 460 core

in vec4 vertexColor;

out vec4 FragColor;

void main()
{
	float diag = abs(gl_PointCoord.x - gl_PointCoord.y)/2 + 0.5;
	FragColor = vertexColor * vec4(diag, diag ,diag, 1.0);
}
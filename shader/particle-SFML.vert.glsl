void main() {
	// transform the vertex position
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	// gl_Position.xy = gl_Position.xy * (1. + 0.2*length(gl_Position.xy));

	// transform the texture coordinates
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

	// forward the vertex color
	gl_FrontColor = gl_Color;
}
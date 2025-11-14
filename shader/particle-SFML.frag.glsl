uniform sampler2D texture;

void main() {
	// lookup the pixel in the texture
	vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);

	// multiply it by the color
	gl_FragColor = gl_Color * pixel;
	gl_FragColor.a = gl_FragColor.a * (1. - smoothstep(0.45, 0.5, distance(gl_TexCoord[0].xy, vec2(0.5, 0.5))) );
}

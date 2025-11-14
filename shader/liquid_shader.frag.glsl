uniform sampler2D texture;
uniform vec4 color;

void main() {
	// lookup the pixel in the texture
	vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);

	// float dist = 2.*distance(gl_TexCoord[0].xy, vec2(0.5, 0.5));
	// float ring = smoothstep(0.5, 0.68, dist) * (1. - smoothstep(0.72, 0.9, dist));
	// float glow = (0.7-abs(dist - 0.7)) * (1. - smoothstep(0.7, 0.99, dist));

	// multiply it by the color
	gl_FragColor = pixel * color;
	gl_FragColor.a = gl_Color.a * gl_FragColor.a * (1. - smoothstep(0., 0.5, distance(gl_TexCoord[0].xy, vec2(0.5, 0.5))) );
}
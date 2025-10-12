#version 460 core
layout (location = 1) out vec4 outAcc; /* RGBA buffer */
layout (location = 2) out float outReveal; /* Reveal buffer */

in flat vec3 normal;
in vec2 texPos;

uniform sampler2D atlas;

void main() {
	vec4 texColor = texture(atlas, texPos);

	outAcc = vec4(texColor.rgb * texColor.a, texColor.a);
	outReveal = texColor.a;
}

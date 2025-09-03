#version 460 core
layout (location = 0) out vec4 FragColor;

in flat vec3 normal;
in vec2 texPos;

uniform sampler2D atlas;

void main() {
	FragColor = vec4(texture(atlas, texPos).rgb, 1.0);
}

#version 460 core
out vec4 FragColor;

in vec2 texPos;

uniform sampler2D atlas;

void main() {
	FragColor = texture(atlas, texPos);
}

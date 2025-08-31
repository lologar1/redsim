#version 460 core
out vec4 FragColor;
in vec2 texPos;

uniform sampler2D texture0;
uniform sampler2D texture1;

void main() {
    FragColor = mix(texture(texture0, texPos), texture(texture1, texPos), 0.5);
}

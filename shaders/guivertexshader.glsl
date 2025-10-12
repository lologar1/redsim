#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexPos;

out vec2 texPos;

uniform mat4 projectionMatrix;

void main() {
	gl_Position = projectionMatrix * vec4(aPos.x, aPos.y, -aPos.z, 1.0);
	texPos = aTexPos;
}

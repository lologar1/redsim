#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexPos;

out flat vec3 normal; /* Send normal for shadow computation */
out vec2 texPos;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main() {
	gl_Position = projectionMatrix * viewMatrix * vec4(aPos, 1.0);
	normal = aNormal; /* Pass it on since mesh is already properly setup */
	texPos = aTexPos; /* Pass interpolated texture atlas coords */
}

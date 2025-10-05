#version 460 core
out vec4 FragColor;

in vec2 texPos;

uniform sampler2D opaqueBuffer;
uniform sampler2D accBuffer;
uniform sampler2D revealBuffer;

void main() {
	vec3 background = texture(opaqueBuffer, texPos).rgb;
	vec4 acc = texture(accBuffer, texPos);
	float reveal = texture(revealBuffer, texPos).r;
	float transAlpha = 1.0 - reveal;
	vec3 transColor = acc.rgb / max(acc.a, 1e-6);

	FragColor = vec4(transColor * transAlpha + background * reveal, 1.0);
}

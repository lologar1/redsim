#version 460 core
out vec4 FragColor;

in vec2 texPos;

uniform sampler2D opaqueBuffer;
uniform sampler2D accBuffer;
uniform sampler2D revealBuffer;
uniform sampler2D guiBuffer;

void main() {
	/* Buffers to mix together */
	vec3 background = texture(opaqueBuffer, texPos).rgb;
	vec4 acc = texture(accBuffer, texPos);
	vec4 gui = texture(guiBuffer, texPos);

	float reveal = texture(revealBuffer, texPos).r; /* revealBuffer only has one component */
	vec3 transColor = acc.rgb / max(acc.a, 1e-6);

	FragColor = vec4(mix(mix(transColor, background, reveal), gui.rgb, gui.a), 1.0);
}

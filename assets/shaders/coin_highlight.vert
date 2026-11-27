#version 120

#common vertex

// Stuff passed to the fragment shader
varying vec2 vTextureCoord0;
varying vec4 vVertexColor;

void main(void)
{
	gl_Position = uWVPMatrix * vec4(inVertexPosition, 1.0);
	gl_PointSize = 1.0;

	vec4 TextureCoord0 = vec4(inTexCoord0.x, inTexCoord0.y, 1.0, 1.0);
	vTextureCoord0 = vec4(uTMatrix0 * TextureCoord0).xy;

	float amp = 0.6 + 0.7 * sin(uRelTime * 1.5 + inVertexPosition.x * 10.0 - inVertexPosition.y * 7.0);
	amp = max(amp, 1.0);
	vVertexColor = vec4(amp, amp, amp, 1.0); // rgba
}

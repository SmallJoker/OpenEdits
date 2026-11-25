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

	vVertexColor = inVertexColor_raw.bgra;
	float fade = mod(uRelTime, 4.0);
	if (fade > 2.0)
		fade = 4.0 - fade;

	vVertexColor.r = fade;
}

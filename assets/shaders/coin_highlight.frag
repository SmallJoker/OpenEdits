#version 120

uniform sampler2D uTextureUnit0;

varying vec2 vTextureCoord0;
varying vec4 vVertexColor;

void main(void)
{
	vec4 color = vVertexColor;
	color *= texture2D(uTextureUnit0, vTextureCoord0);

	gl_FragColor = color;
}

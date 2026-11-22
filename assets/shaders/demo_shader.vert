#version 120

// TODO: Move this header to C++ (template)

#if 1 // video::EDT_OPENGL

#define inVertexPosition gl_Vertex.xyz
#define inVertexNormal gl_Normal
#define inVertexColor_raw gl_Color
#define inTexCoord0 gl_MultiTexCoord0

#else // OpenGL3 and OpenGL ES

// Available attributes: see Irrlicht "sBuiltInVertexAttributeNames"
attribute vec3 inVertexPosition;
attribute vec3 inVertexNormal;
attribute vec4 inVertexColor_raw;
attribute vec2 inTexCoord0;
#endif

// Uniforms (from C++)
uniform mat4 uWVPMatrix; // gl_ModelViewProjectionMatrix
uniform mat4 uTMatrix0;
uniform float uRelTime;

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

#version 300 es
precision highp float;

uniform mat4 u_proj_mtx;

in vec2 in_position;
in vec2 in_texUV;
in int in_texID;

flat out int v_texID;
out vec2 v_texUV;

void
main()
{
	v_texID = in_texID;
	v_texUV = in_texUV;
	gl_Position = u_proj_mtx * vec4(in_position, 0.0, 1.0);
}

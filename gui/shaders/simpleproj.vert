#version 300 es

precision mediump float;

uniform mat4 proj_mtx;
in vec2 position;

void
main()
{
	gl_Position = proj_mtx * vec4(position.xy, 0, 1);
}

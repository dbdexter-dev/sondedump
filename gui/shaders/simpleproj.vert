#version 300 es

precision mediump float;

uniform mat4 u_proj_mtx;

in float in_position_x;
in float in_position_y;

void
main()
{
	gl_Position = u_proj_mtx * vec4(in_position_x, in_position_y, 0.0, 1.0);
	gl_PointSize = 4.0f;
}

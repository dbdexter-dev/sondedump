#version 300 es

precision highp float;

uniform mat4 u_proj_mtx;

in float in_position_x;
in float in_position_y;

void
main()
{
	float pi = 3.1415926536;
	float rads = in_position_x * pi / 180.0;

	float x = (in_position_y + 180.0) / 360.0;
	float y = (1.0 - log(tan(rads) + 1.0/cos(rads)) / pi) / 2.0;

	gl_Position = u_proj_mtx * vec4(x, y, 0.0, 1.0);
	gl_PointSize = 4.0f;
}

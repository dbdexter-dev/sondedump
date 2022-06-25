#version 300 es

precision mediump float;

uniform mat4 proj_mtx;
uniform float zoom;

in vec2 position;

void
main()
{
	float pi = 3.1415926536;
	float rads = position.x * pi / 180.0;

	float x = zoom * (position.y + 180.0) / 360.0;
	float y = zoom * (1.0 - log(tan(rads) + 1.0/cos(rads)) / pi) / 2.0;

	gl_Position = proj_mtx * vec4(x, y, 0, 1);
	gl_PointSize = 4.0f;
}

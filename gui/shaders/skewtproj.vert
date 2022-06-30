#version 300 es

precision mediump float;

uniform mat4 proj_mtx;
//in vec2 position;
in float in_temp;
in float in_altitude;

void
main()
{
	gl_Position = proj_mtx * vec4(in_temp, log(in_altitude), 0.0, 1.0);
	gl_PointSize = 4.0f;
}

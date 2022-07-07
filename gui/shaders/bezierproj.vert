#version 300 es

precision highp float;

uniform mat4 u_proj_mtx;
uniform float u_thickness;
uniform float u_zoom;

in vec2 in_position;
in vec2 in_normal;

out vec2 v_normal;

void
main()
{
	/* Apply offset to each point to create thickness */
	vec2 delta = in_normal * u_thickness;

	/* Generate shader outputs */
	v_normal = in_normal;
	gl_Position = u_proj_mtx * vec4(in_position + delta, 0.0, 1.0);
}

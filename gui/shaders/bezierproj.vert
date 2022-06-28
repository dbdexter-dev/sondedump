#version 300 es

precision highp float;

uniform mat4 u_proj_mtx;
uniform float u_thickness;
uniform float u_zoom;

in vec2 in_p0, in_p1, in_p2, in_p3; /* control points, not an array cause ES profile */
in float in_t;                      /* abs(t) \in [0, 1], sign determines normal direction */

out vec2 v_normal;

/**
 * Evaluate cubic bezier at given point
 * @param t     interpolation index, \in [0, 1]
 * @param p0..3 control points
 */
vec2
bezier(float t, vec2 p0, vec2 p1, vec2 p2, vec2 p3)
{
	/* 4 -> 3 interpolation */
	vec2 p01 = mix(p0, p1, t);
	vec2 p12 = mix(p1, p2, t);
	vec2 p23 = mix(p2, p3, t);

	/* 3 -> 2 interpolation */
	vec2 p012 = mix(p01, p12, t);
	vec2 p123 = mix(p12, p23, t);

	/* 2 -> 1 interpolation */
	return mix(p012, p123, t);
}

/**
 * Evaluate bezier derivative at given point
 * @param t     interpolation index, \in [0, 1]
 * @param p0..3 control points
 */
vec2
bezier_deriv(float t, vec2 p0, vec2 p1, vec2 p2, vec2 p3)
{
	vec2 a = p3 - 3.0 * p2 + 3.0 * p1 - p0;
	vec2 b = 3.0 * (p0 - 2.0 * p1 + p2);
	vec2 c = 3.0 * (p1 - p0);

	return 3.0 * (t*t) * a + 2.0 * t * b + c;
}

void
main()
{
	/* Extract t from input */
	float abs_t = abs(in_t) - 1.0;
	vec2 position = bezier(abs_t, in_p0, in_p1, in_p2, in_p3);

	/* Compute curve normal at current position */
	vec2 derivative = bezier_deriv(abs_t, in_p0, in_p1, in_p2, in_p3);
	vec2 normal = normalize(vec2(-derivative.y, derivative.x)) * sign(in_t);

	/* Apply offset to each point to create thickness */
	vec2 delta = normal * u_thickness;

	/* Generate shader outputs */
	v_normal = normal;
	gl_Position = u_proj_mtx * vec4(position + delta, 0.0, 1.0);
}

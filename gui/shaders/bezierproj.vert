#version 300 es

precision mediump float;

uniform mat4 proj_mtx;
uniform vec2 control_points[4];
uniform float thickness;

/* abs(t) \in [0, 1], sign determines normal direction */
in float t;

vec2
bezier(float t, vec2 p[4])
{
	/* 4 -> 3 interpolation */
	vec2 p01 = mix(p[0], p[1], t);
	vec2 p12 = mix(p[1], p[2], t);
	vec2 p23 = mix(p[2], p[3], t);

	/* 3 -> 2 interpolation */
	vec2 p012 = mix(p01, p12, t);
	vec2 p123 = mix(p12, p23, t);

	/* 2 -> 1 interpolation */
	return mix(p012, p123, t);
}

vec2
bezier_deriv(float t, vec2 p[4])
{
	vec2 a = p[3] - 3.0 * p[2] + 3.0 * p[1] - p[0];
	vec2 b = 3.0 * (p[0] - 2.0 * p[1] + p[2]);
	vec2 c = 3.0 * (p[1] - p[0]);

	return 3.0 * (t*t) * a + 2.0 * t * b + c;
}


void
main()
{
	vec2 position = bezier(abs(t), control_points);
	vec2 derivative = bezier_deriv(abs(t), control_points);
	vec2 normal = normalize(vec2(-derivative.y, derivative.x)) * sign(t);
	vec2 delta = normal * thickness;

	gl_Position = proj_mtx * vec4(position + delta, 0, 1);
}

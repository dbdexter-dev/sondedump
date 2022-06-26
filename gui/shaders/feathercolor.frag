#version 300 es

precision mediump float;

uniform vec4 color;

in vec2 v_normal;
flat in float v_thickness;
out vec4 out_color;

void
main()
{
	/* AA: Modulate alpha so that it goes from 0% at the edge to 100% 1/4 of the way into the line */
	float alpha = smoothstep(0.0, 1.0 / v_thickness, 1.0 - length(v_normal));

	out_color = vec4(color.xyz, alpha);
}

#version 300 es

#define AA_BORDER_WIDTH_PX 1.0

precision mediump float;

uniform vec4 u_color;
uniform float u_aa_thickness;

in vec2 v_normal;               /* Normal to the line direction, -1..1 */
out vec4 out_color;

void
main()
{
	/* AA: Modulate alpha so that it goes from 0% at the edge to 100% 1 pixel
	 * into the line */
	float alpha = smoothstep(0.0, AA_BORDER_WIDTH_PX / u_aa_thickness, 1.0 - length(v_normal));

	out_color = vec4(u_color.xyz, alpha);
}

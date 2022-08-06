#version 300 es
precision mediump float;
precision mediump sampler2DArray;

uniform sampler2DArray u_texture;

flat in int v_texID;
in vec2 v_texUV;

out vec4 out_color;

void
main()
{
	if (v_texID < 0) {
		out_color = vec4(0);
	} else {
		out_color = texture(u_texture, vec3(v_texUV, v_texID));
	}
}

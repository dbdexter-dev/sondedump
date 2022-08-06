#include "gl_utils.h"

GLint
gl_compile_and_link(GLuint *program, const char *vert_src, int vsize, const char *frag_src, int fsize)
{
	GLint status;
	GLuint vert_shader, frag_shader;

	*program = glCreateProgram();
	vert_shader = glCreateShader(GL_VERTEX_SHADER);
	frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vert_shader, 1, &vert_src, &vsize);
	glShaderSource(frag_shader, 1, &frag_src, &fsize);

	glCompileShader(vert_shader);
	glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) return status;

	glCompileShader(frag_shader);
	glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) return status;

	glAttachShader(*program, vert_shader);
	glAttachShader(*program, frag_shader);
	glLinkProgram(*program);

	glDetachShader(*program, vert_shader);
	glDetachShader(*program, frag_shader);
	glDeleteShader(vert_shader);
	glDeleteShader(frag_shader);

	glGetProgramiv(*program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) return status;

	return GL_TRUE;
}

#ifndef gl_utils_h
#define gl_utils_h

#include <stdlib.h>
#include "libs/glad/glad.h"

/**
 * Compile and link the given shader sources into an executable, and return the
 * resulting program
 *
 * @param program pointer to the program handle to generate
 * @param vert_src vertex shader source
 * @param vsize size of the vertex shader source
 * @param frag_src fragment shader source
 * @param fsize size of the fragment shader source
 * @return GL_TRUE on success,
 */
GLint gl_compile_and_link(GLuint *program, const char *vert_src, int vsize, const char *frag_src, int fsize);


#endif


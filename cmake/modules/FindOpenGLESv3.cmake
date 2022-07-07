# Try to find OpenGL ES v3
# Once done this will define
#   OPENGLES3_FOUND - system has OpenGLESv3
#   OPENGLES3_INCLUDE_DIRS - OpenGLESv3 include directories
#   OPENGLES3_LIBRARIES - Link these to use OpenGLESv3


if (OPENGLES3_LIBRARIES AND OPENGLES3_INCLUDE_DIRS)
	set(OPENGLES3_FOUND TRUE)
else()
	find_path(OPENGLES3_INCLUDE_DIR
		NAMES GLES3/gl3.h
		PATHS /usr/include /usr/local/include /opt/local/include
	)

	find_library(OPENGLES3_LIBRARY
		NAMES GLESv2
		PATHS /usr/lib /usr/local/lib /opt/local/lib /lib
	)

	set(OPENGLES3_INCLUDE_DIRS ${OPENGLES3_INCLUDE_DIR})
	set(OPENGLES3_LIBRARIES ${OPENGLES3_LIBRARY})

	if (OPENGLES3_INCLUDE_DIRS AND OPENGLES3_LIBRARIES)
		set(OPENGLES3_FOUND TRUE)
	endif()

	if (OPENGLES3_FOUND)
		if (NOT OpenGLESv3_FIND_QUIETLY)
			message(STATUS "Found OpenGL ES: ${OPENGLES3_LIBRARIES}")
		endif()
	else()
		if (OpenGLESv3_FIND_REQUIRED)
			message(FATAL_ERROR "Could not find OpenGL ES")
		else()
			message(STATUS "Could not find OpenGL ES")
		endif()
	endif()

	mark_as_advanced(OPENGLESV3_INCLUDE_DIRS OPENGLESV3_LIBRARIES)

endif()

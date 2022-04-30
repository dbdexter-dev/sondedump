cmake_minimum_required(VERSION 3.10)

option(ENABLE_TUI "Enable Ncurses TUI (requires ncurses)" ON)
option(ENABLE_GUI "Enable Nuklear GUI (requires OpenGL and SDL2)" ON)
option(ENABLE_AUDIO "Enable audio input (requires portaudio)" ON)
option(UNINSTALL_TARGET "Generate uninstall target" ON)

project(sondedump
	VERSION 0.7
	DESCRIPTION "Radiosonde decoder"
	LANGUAGES C)
add_definitions(-DVERSION="${CMAKE_PROJECT_VERSION}")

# Include modules for external libraries
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

if (MSVC)
	set(CMAKE_C_STANDARD 11)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wall /std:c11")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ftree-vectorize")
else()
	set(CMAKE_C_STANDARD 99)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -pipe -Wall -Wextra -pedantic -Wimplicit-fallthrough")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ftree-vectorize")
endif()

# ARM architectures need -mfpu=auto in order to enable NEON when available,
# but that option is unrecognized by x86 gcc (and possibly others): only
# add it to the release flags when the compiler's target is arm
# This is not a problem for arm64, as NEON support is mandatory for that arch
execute_process(COMMAND "${CMAKE_C_COMPILER}" "-dumpmachine" COMMAND "grep" "arm" OUTPUT_QUIET RESULT_VARIABLE is_arm)
if (is_arm EQUAL "0")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mcpu=native -mfpu=auto")
endif()


set(LIBRARY_SOURCES
	decode/correlator/correlator.c decode/correlator/correlator.h

	demod/dsp/filter.c demod/dsp/filter.h
	demod/dsp/timing.c demod/dsp/timing.h
	demod/dsp/agc.c demod/dsp/agc.h
	demod/gfsk.c demod/gfsk.h
	demod/afsk.c demod/afsk.h

	decode/manchester.c decode/manchester.h
	decode/framer.c decode/framer.h
	decode/xdata.c decode/xdata.h

	decode/ecc/crc.c decode/ecc/crc.h
	decode/ecc/rs.c decode/ecc/rs.h

	sonde/rs41/rs41.c
	sonde/rs41/frame.c sonde/rs41/frame.h
	sonde/rs41/subframe.c sonde/rs41/subframe.h
	sonde/rs41/protocol.h

	sonde/dfm09/dfm09.c
	sonde/dfm09/frame.c sonde/dfm09/frame.h
	sonde/dfm09/subframe.c sonde/dfm09/subframe.h
	sonde/dfm09/protocol.h

	sonde/ims100/ims100.c
	sonde/ims100/frame.c sonde/ims100/frame.h
	sonde/ims100/subframe.c sonde/ims100/subframe.h
	sonde/ims100/protocol.h

	sonde/m10/m10.c
	sonde/m10/frame.c sonde/m10/frame.h
	sonde/m10/subframe.c sonde/m10/subframe.h
	sonde/m10/protocol.h

	sonde/imet4/imet4.c
	sonde/imet4/frame.c sonde/imet4/frame.h
	sonde/imet4/subframe.c sonde/imet4/subframe.h
	sonde/imet4/protocol.h

	include/dfm09.h
	include/m10.h
	include/imet4.h
	include/ims100.h
	include/rs41.h
	include/data.h

	gps/ecef.c gps/ecef.h
	gps/time.c gps/time.h

	bitops.c bitops.h
	physics.c physics.h
	utils.c utils.h
)

set(TUI_SOURCES
	tui/tui.c tui/tui.h
)

set(AUDIO_SOURCES
	io/audio.c io/audio.h
)

set(GUI_SOURCES
	gui/nuklear/nuklear.c gui/nuklear/nuklear.h
	gui/nuklear/nuklear_sdl_gl3.h

	gui/gui.c gui/gui.h
)

set(EXEC_SOURCES
	io/gpx.c io/gpx.h
	io/kml.c io/kml.h
	io/wavfile.c io/wavfile.h

	main.c
)

set(COMMON_INC_DIRS ${PROJECT_SOURCE_DIR})

if (MSVC)
	set(EXEC_SOURCES ${EXEC_SOURCES}
		win_getopt.c win_getopt.h
	)
endif()

# Portable math
if (NOT WIN32)
	set(MATH_LIBRARY "m")
endif()

# Find Curses/Ncurses
find_package(Curses)
if (ENABLE_TUI AND CURSES_FOUND)
	# Portable threads
	find_package(Threads REQUIRED)

	add_definitions(-DENABLE_TUI)
	set(EXEC_SOURCES ${EXEC_SOURCES} ${TUI_SOURCES})
else()
	set(CURSES_LIBRARIES "")
	set(CURSES_INCLUDE_DIRS "")
endif()

# Find PortAudio
find_package(Portaudio)
if(ENABLE_AUDIO AND PORTAUDIO_FOUND)
	add_definitions(-DENABLE_AUDIO)
	set(EXEC_SOURCES ${EXEC_SOURCES} ${AUDIO_SOURCES})
else()
	set(PORTAUDIO_LIBRARIES "")
	set(PORTAUDIO_INCLUDE_DIRS "")
endif()

# Find OpenGL and SDL2
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL)
find_package(SDL2)
find_package(GLEW)
if (ENABLE_GUI AND OPENGL_FOUND AND SDL2_FOUND AND GLEW_FOUND)
	# Nuklear configuration, has to be consistent every time nuklear.h is included
	add_definitions(
		-DNK_INCLUDE_FIXED_TYPES
		-DNK_INCLUDE_DEFAULT_ALLOCATOR
		-DNK_INCLUDE_STANDARD_IO
		-DNK_INCLUDE_STANDARD_VARARGS
		-DNK_INCLUDE_FONT_BAKING
		-DNK_INCLUDE_DEFAULT_FONT
		-DNK_INCLUDE_VERTEX_BUFFER_OUTPUT
	)
	add_definitions(-DENABLE_GUI)
	set(EXEC_SOURCES ${EXEC_SOURCES} ${GUI_SOURCES})
else()
	set(SDL2_LIBRARIES "")
	set(SDL2_INCLUDE_DIRS "")
	set(SDL2_LIBRARY_DIRS "")
	set(OPENGL_INCLUDE_DIRS "")
	set(OPENGL_LIBRARIES "")
	set(OPENGL_LIBRARY_DIRS "")
	set(GLEW_INCLUDE_DIRS "")
	set(GLEW_LIBRARIES "")
	set(GLEW_LIBRARY_DIRS "")
endif()


# Main library target
add_library(radiosonde STATIC ${LIBRARY_SOURCES})
target_include_directories(radiosonde PUBLIC ${COMMON_INC_DIRS})
target_link_libraries(radiosonde PUBLIC ${MATH_LIBRARY})

# Main executable target
add_executable(sondedump ${EXEC_SOURCES})
target_include_directories(sondedump PRIVATE ${COMMON_INC_DIRS}
	${PORTAUDIO_INCLUDE_DIRS}
	${CURSES_INCLUDE_DIRS}
	${SDL2_INCLUDE_DIRS}
	${OPENGL_INCLUDE_DIRS}
	${GLEW_INCLUDE_DIRS}
)
target_link_libraries(sondedump PRIVATE radiosonde
	${CMAKE_THREAD_LIBS_INIT}
	${MATH_LIBRARY}
	${PORTAUDIO_LIBRARIES}
	${CURSES_LIBRARIES}
	${SDL2_LIBRARIES}
	${GLEW_LIBRARIES}
	OpenGL::GL
)

# Install targets
install(TARGETS sondedump DESTINATION bin)

# uninstall target
if(UNINSTALL_TARGET AND NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()


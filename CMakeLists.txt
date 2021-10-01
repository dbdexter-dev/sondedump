cmake_minimum_required(VERSION 3.10)

option(ENABLE_CAIRO "Enable thermodynamic diagrams" ON)
option(ENABLE_GUI "Enable GUI" ON)

project(sondedump
	VERSION 0.1
	DESCRIPTION "Radiosonde decoder"
	LANGUAGES C)
add_definitions(-DVERSION="${CMAKE_PROJECT_VERSION}")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -pipe -Wextra -Wimplicit-fallthrough")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native -ffast-math -ftree-vectorize")

# ARM architectures need -mfpu=auto in order to enable NEON when available,
# but that option is unrecognized by x86 gcc (and possibly others): only
# add it to the release flags when the compiler's target is arm
# This is not a problem for arm64, as NEON support is mandatory for that arch
execute_process(COMMAND "${CMAKE_C_COMPILER}" "-dumpmachine" COMMAND "grep" "arm" OUTPUT_QUIET RESULT_VARIABLE is_arm)
if (is_arm EQUAL "0")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mcpu=native -mfpu=auto")
endif()


set(LIBRARY_SOURCES
	correlator/correlator.c correlator/correlator.h

	dsp/filter.c dsp/filter.h
	dsp/timing.c dsp/timing.h
	dsp/agc.c dsp/agc.h

	demod/gfsk.c demod/gfsk.h

	decode/prng.c decode/prng.h
	decode/common.h

	decode/rs41/rs41.c decode/rs41/rs41.h
	decode/rs41/frame.c decode/rs41/frame.h
	decode/rs41/subframe.c decode/rs41/subframe.h
	decode/rs41/protocol.h

	ecc/crc.c ecc/crc.h
	ecc/rs.c ecc/rs.h

	gps/ecef.c gps/ecef.h
	gps/time.c gps/time.h

	io/gpx.c io/gpx.h
	io/kml.c io/kml.h
	io/wavfile.c io/wavfile.h

	utils.c utils.h
)

set(CAIRO_SOURCES
	diagrams/stuve.c diagrams/stuve.h
)

set(TUI_SOURCES
	tui/tui.c tui/tui.h
)

set(EXEC_SOURCES
	main.c
)

set(COMMON_INC_DIRS
	${PROJECT_SOURCE_DIR}
)

# Extra functionality libraries
find_library(CAIRO_LIBRARY NAMES cairo libcairo OPTIONAL)
if (CAIRO_LIBRARY)
	add_definitions(-DENABLE_DIAGRAMS)
	set(EXEC_SOURCES ${EXEC_SOURCES} ${CAIRO_SOURCES})
endif()
find_library(NCURSES_LIBRARY NAMES ncursesw libncursesw ncurses libncurses OPTIONAL)
if (NCURSES_LIBRARY)
	add_definitions(-DENABLE_TUI)
	set(EXEC_SOURCES ${EXEC_SOURCES} ${TUI_SOURCES})
endif()


# Main library target
add_library(sondedump_static STATIC ${LIBRARY_SOURCES})
target_include_directories(sondedump_static PRIVATE ${COMMON_INC_DIRS})
target_link_libraries(sondedump_static PRIVATE m)

# Shared library target
add_library(sonde SHARED ${LIBRARY_SOURCES})
target_include_directories(sonde PRIVATE ${COMMON_INC_DIRS})
set_target_properties(sonde PROPERTIES EXCLUDE_FROM_ALL 1)

# Main executable target
add_executable(sondedump ${EXEC_SOURCES})
target_include_directories(sondedump PRIVATE ${COMMON_INC_DIRS})
target_link_libraries(sondedump PRIVATE m pthread sondedump_static ${CAIRO_LIBRARY} ${NCURSES_LIBRARY})

# Install targets
install(TARGETS sondedump DESTINATION bin)

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()


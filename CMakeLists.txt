cmake_minimum_required(VERSION 3.10)

option(ENABLE_TUI "Enable Ncurses TUI (requires ncurses)" ON)
option(ENABLE_AUDIO "Enable audio input (requires portaudio)" ON)

project(sondedump
	VERSION 0.4
	DESCRIPTION "Radiosonde decoder"
	LANGUAGES C)
add_definitions(-DVERSION="${CMAKE_PROJECT_VERSION}")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -pipe -Wall -Wextra -Wimplicit-fallthrough")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native -ftree-vectorize")

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

	decode/manchester.c decode/manchester.h
	decode/framer.c decode/framer.h
	decode/xdata.c decode/xdata.h
	decode/common.h

	decode/ecc/crc.c decode/ecc/crc.h
	decode/ecc/rs.c decode/ecc/rs.h

	sonde/rs41/rs41.c sonde/rs41/rs41.h
	sonde/rs41/frame.c sonde/rs41/frame.h
	sonde/rs41/subframe.c sonde/rs41/subframe.h
	sonde/rs41/protocol.h

	sonde/dfm09/dfm09.c sonde/dfm09/dfm09.h
	sonde/dfm09/frame.c sonde/dfm09/frame.h
	sonde/dfm09/subframe.c sonde/dfm09/subframe.h
	sonde/dfm09/protocol.h

	sonde/m10/m10.c sonde/m10/m10.h
	sonde/m10/frame.c sonde/m10/frame.h
	sonde/m10/protocol.c sonde/m10/protocol.h

	sonde/ims100/ims100.c sonde/ims100/ims100.h
	sonde/ims100/frame.c sonde/ims100/frame.h
	sonde/ims100/subframe.c sonde/ims100/subframe.h
	sonde/ims100/protocol.c sonde/ims100/protocol.h

	gps/ecef.c gps/ecef.h
	gps/time.c gps/time.h

	io/gpx.c io/gpx.h
	io/kml.c io/kml.h
	io/wavfile.c io/wavfile.h

	utils.c utils.h
)

set(TUI_SOURCES
	tui/tui.c tui/tui.h
)

set(AUDIO_SOURCES
	io/audio.c io/audio.h
)

set(EXEC_SOURCES
	main.c
)

set(COMMON_INC_DIRS
	${PROJECT_SOURCE_DIR}
)

# Extra functionality libraries
message("\nBuild configuration:")
find_library(NCURSES_LIBRARY NAMES ncursesw libncursesw ncurses libncurses OPTIONAL)
if (ENABLE_TUI AND NCURSES_LIBRARY)
	add_definitions(-DENABLE_TUI)
	set(EXEC_SOURCES ${EXEC_SOURCES} ${TUI_SOURCES})
	message("   ncurses     ON")
else()
	set(NCURSES_LIBRARY "")
	message("   ncurses     OFF")
endif()
find_library(PORTAUDIO_LIBRARY NAMES portaudio libportaudio OPTIONAL)
if(ENABLE_AUDIO AND PORTAUDIO_LIBRARY)
	add_definitions(-DENABLE_AUDIO)
	set(EXEC_SOURCES ${EXEC_SOURCES} ${AUDIO_SOURCES})
	message("   portaudio   ON")
else()
	set(PORTAUDIO_LIBRARY "")
	message("   portaudio   OFF")
endif()
message("")


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
target_link_libraries(sondedump PRIVATE m pthread sondedump_static
	${PORTAUDIO_LIBRARY} ${CAIRO_LIBRARY} ${NCURSES_LIBRARY})

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


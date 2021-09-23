cmake_minimum_required(VERSION 3.10)

project(sondedump
	VERSION 0.1
	DESCRIPTION "Radiosonde decoder"
	LANGUAGES C)
add_definitions(-DVERSION="${CMAKE_PROJECT_VERSION}")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wextra -Wimplicit-fallthrough")
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

	decode/rs41/rs41.c decode/rs41/rs41.h
	decode/rs41/frame.c decode/rs41/frame.h
	decode/rs41/subframe.c decode/rs41/subframe.h
	decode/rs41/protocol.h

	ecc/crc.c ecc/crc.h
	ecc/rs.c ecc/rs.h

	wavfile.c wavfile.h
	utils.c utils.h
)

set(COMMON_INC_DIRS
	${PROJECT_SOURCE_DIR}
)


# Main library target
add_library(sondedump_static STATIC ${LIBRARY_SOURCES})
target_include_directories(sondedump_static PRIVATE ${COMMON_INC_DIRS})
target_link_libraries(sondedump_static PRIVATE m)

# Shared library target
add_library(sonde SHARED ${LIBRARY_SOURCES})
target_include_directories(sonde PRIVATE ${COMMON_INC_DIRS})
set_target_properties(sonde PROPERTIES EXCLUDE_FROM_ALL 1)

# Main executable target
add_executable(sondedump main.c ${EXEC_SOURCES})
target_include_directories(sondedump PRIVATE ${COMMON_INC_DIRS})
target_link_libraries(sondedump PRIVATE m sondedump_static)

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


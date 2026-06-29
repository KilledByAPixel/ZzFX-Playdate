# CMakeLists.txt for the ZzFX Playdate demo (recommended on Windows).
#
# Simulator build (Windows, needs Visual Studio or MinGW + CMake):
#   mkdir build
#   cd build
#   cmake ..
#   cmake --build .
#
# Device build (needs the ARM GNU toolchain, arm-none-eabi-gcc):
#   mkdir build-device & cd build-device
#   cmake .. -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=%PLAYDATE_SDK_PATH%/C_API/buildsupport/arm.cmake
#   cmake --build .
#
cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 11)

set(ENVSDK $ENV{PLAYDATE_SDK_PATH})

if (NOT ${ENVSDK} STREQUAL "")
	# Convert path from Windows
	file(TO_CMAKE_PATH ${ENVSDK} SDK)
else()
	execute_process(
		COMMAND bash -c "egrep '^\\s*SDKRoot' $HOME/.Playdate/config"
		COMMAND head -n 1
		COMMAND cut -c9-
		OUTPUT_VARIABLE SDK
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif()

if (NOT EXISTS ${SDK})
	message(FATAL_ERROR "SDK Path not found; set the PLAYDATE_SDK_PATH environment variable")
	return()
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release")
set(CMAKE_XCODE_GENERATE_SCHEME TRUE)

# Game name
set(PLAYDATE_GAME_NAME ZzFX)
set(PLAYDATE_GAME_DEVICE ZzFX_DEVICE)

project(${PLAYDATE_GAME_NAME} C ASM)

if (TOOLCHAIN STREQUAL "armgcc")
	add_executable(${PLAYDATE_GAME_DEVICE} ${SDK}/C_API/buildsupport/setup.c src/main.c src/zzfx.c)
else()
	add_library(${PLAYDATE_GAME_NAME} SHARED src/main.c src/zzfx.c)
endif()

include(${SDK}/C_API/buildsupport/playdate_game.cmake)

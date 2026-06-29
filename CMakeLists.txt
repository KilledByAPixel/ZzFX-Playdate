cmake_minimum_required(VERSION 3.14)
set(CMAKE_C_STANDARD 11)

set(ENVSDK $ENV{PLAYDATE_SDK_PATH})
if (NOT "${ENVSDK}" STREQUAL "")
	file(TO_CMAKE_PATH "${ENVSDK}" SDK)
endif()

if ("${SDK}" STREQUAL "")
	set(CONFIG_CANDIDATES
		"$ENV{HOME}/.Playdate/config"
		"$ENV{USERPROFILE}/.Playdate/config"
	)
	foreach(CONFIG_PATH IN LISTS CONFIG_CANDIDATES)
		if (EXISTS "${CONFIG_PATH}")
			file(STRINGS "${CONFIG_PATH}" SDKROOT_LINE REGEX "^[ \t]*SDKRoot")
			if (SDKROOT_LINE)
				string(REGEX REPLACE "^[ \t]*SDKRoot[ \t]*" "" SDK "${SDKROOT_LINE}")
				string(STRIP "${SDK}" SDK)
				file(TO_CMAKE_PATH "${SDK}" SDK)
				break()
			endif()
		endif()
	endforeach()
endif()

if ("${SDK}" STREQUAL "" OR NOT EXISTS "${SDK}")
	message(FATAL_ERROR "Playdate SDK path not found. Set PLAYDATE_SDK_PATH or configure ~/.Playdate/config.")
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

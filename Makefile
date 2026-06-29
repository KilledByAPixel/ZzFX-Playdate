# Makefile for the ZzFX Playdate demo (combined C + Lua project).
#
#   make           -> build for the Simulator  (build/ + ZzFX.pdx)
#   make device    -> build for Playdate hardware
#   make run       -> build + open in the Simulator (macOS)
#   make clean
#
# Requires the Playdate SDK. The SDK is located via $PLAYDATE_SDK_PATH or your
# ~/.Playdate/config. For device builds you also need the ARM toolchain
# (arm-none-eabi-gcc).

HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = ZzFX.pdx

# Locate the SDK
SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
	SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set the PLAYDATE_SDK_PATH environment variable)
endif

# C source files
SRC = src/main.c \
      src/zzfx.c

# Where to find headers (zzfx.h lives in src/)
UINCDIR = src

# Unused, but required to be defined by common.mk
UDEFS   =
UADEFS  =
ULIBDIR =
ULIBS   =

include $(SDK)/C_API/buildsupport/common.mk

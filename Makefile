#####################################################################################################################
# @file Makefile
# @note Copyright (C) [2021-2022] Renesas Electronics Corporation and/or its affiliates
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2, as published
# by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#####################################################################################################################
#####################################################################################################################
# Release Tag: 1-0-2
# Pipeline ID: 118059
# Commit Hash: 5a4424ad
#####################################################################################################################

###################
# V A R I A B L E S
###################

ifndef ESMC_STACK
$(warning ESMC_STACK is not defined)
ESMC_STACK := renesas
$(warning Setting ESMC_STACK to $(ESMC_STACK) (default)...)
endif

# PLATFORM:
#  PLATFORM := amd64 --> AMD64
#  PLATFORM := arm64 --> ARM64
ifndef PLATFORM
$(warning PLATFORM is not defined (PLATFORM must be amd64 or arm64))
PLATFORM := amd64
$(warning Setting PLATFORM to $(PLATFORM))
endif

ifeq ($(PLATFORM),arm64)
override CROSS_COMPILE := /usr/bin/aarch64-linux-gnu-
$(warning Setting CROSS_COMPILE to $(CROSS_COMPILE))
endif

# I2C_DRIVER:
#  I2C_DRIVER := ftdi  --> FTDI
#  I2C_DRIVER := smbus --> SMBus
#  I2C_DRIVER := rsmu  --> RSMU
ifndef I2C_DRIVER
$(warning I2C_DRIVER is not defined (I2C_DRIVER must be ftdi, smbus, or rsmu))
I2C_DRIVER := rsmu
$(warning Setting I2C_DRIVER to $(I2C_DRIVER))
endif

# DEVICE:
#  DEVICE := generic --> Generic device
#  DEVICE := cm      --> ClockMatrix
ifndef DEVICE
$(warning DEVICE is not defined)
DEVICE := generic
$(warning Setting DEVICE to $(DEVICE))
endif

ifeq ($(I2C_DRIVER),rsmu)
override SERIAL_ADDRESS_MODE := 0
$(warning Setting SERIAL_ADDRESS_MODE to $(SERIAL_ADDRESS_MODE))
endif

ifndef SERIAL_ADDRESS_MODE
$(warning SERIAL_ADDRESS_MODE is not defined (SERIAL_ADDRESS_MODE must be either 0, 7, 8, 15, or 16))
SERIAL_ADDRESS_MODE := 8
$(warning Setting SERIAL_ADDRESS_MODE to $(SERIAL_ADDRESS_MODE) (default)...)
endif

ifeq ($(DEVICE),cm) # >>>> Start ClockMatrix definitions
IDT_CLOCKMATRIX_HOST_ADDRESS_MODE := $(SERIAL_ADDRESS_MODE)

ifeq ($(IDT_CLOCKMATRIX_HOST_ADDRESS_MODE),16)
$(error IDT_CLOCKMATRIX_HOST_ADDRESS_MODE=$(IDT_CLOCKMATRIX_HOST_ADDRESS_MODE) is currently disabled)
endif

ifeq ($(IDT_CLOCKMATRIX_HOST_ADDRESS_MODE),$(filter $(IDT_CLOCKMATRIX_HOST_ADDRESS_MODE),7 15))
$(error SPI is not supported)
endif

ifndef REV_ID
$(warning REV_ID is not defined (REV_ID must be either rev-e or rev-d-p))
REV_ID := rev-d-p
$(warning Setting REV_ID to $(REV_ID) (default)...)
endif
endif # <<<< End ClockMatrix definitions

ifndef SYNCE4L_DEBUG_MODE
$(warning SYNCE4L_DEBUG_MODE is not defined (SYNCE4L_DEBUG_MODE must be either 0 or 1))
SYNCE4L_DEBUG_MODE := 0
$(warning Setting SYNCE4L_DEBUG_MODE to $(SYNCE4L_DEBUG_MODE) (default)...)
endif

PROJ_DIR  := .
BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj
PKG_DIR   := pkg

CFG_DIR   := cfg
CFG_FILES := $(shell find $(CFG_DIR) -name "*.cfg")
TCS_FILES := $(shell find $(CFG_DIR) -name "*.tcs")

LICENSE_FILE := COPYING

MAKEFILE := $(MAKEFILE_LIST)

README_FILE := README.md

COMMON_DIR       := common
COMMON_SRC_FILES := $(shell find $(COMMON_DIR) -name "*.c")

CONTROL_DIR       := control
CONTROL_SRC_FILES := $(shell find $(CONTROL_DIR) -name "*.c")

DEVICE_DIR         := device/$(DEVICE)
DEVICE_SRC_FILES   := $(shell find $(DEVICE_DIR) -name "*.c")
DEVICE_ADAPTOR_DIR := device/device_adaptor

ifeq ($(DEVICE),cm) # >>>> Start ClockMatrix definitions
CLOCKMATRIX_API_DIR       := $(DEVICE_DIR)/clockmatrix-api
CLOCKMATRIX_API_INC_DIR   := $(CLOCKMATRIX_API_DIR)/include/$(REV_ID)
CLOCKMATRIX_API_SRC_DIR   := $(CLOCKMATRIX_API_DIR)/source/$(REV_ID)
CLOCKMATRIX_API_SRC_FILES := $(shell find $(CLOCKMATRIX_API_SRC_DIR) -name "*.c")
override DEVICE_SRC_FILES := $(CLOCKMATRIX_API_SRC_FILES) $(shell find $(DEVICE_DIR) -maxdepth 1 -name "*.c")
endif # <<<< End ClockMatrix definitions

I2C_DRIVER_DIR       := device/i2c_drivers/$(I2C_DRIVER)
I2C_DRIVER_SRC_FILES := $(shell find $(I2C_DRIVER_DIR) -name "*.c")

ESMC_STACK_DIR       := esmc/$(ESMC_STACK)
ESMC_STACK_SRC_FILES := $(shell find $(ESMC_STACK_DIR) -name "*.c")
ESMC_ADAPTOR_DIR     := esmc/esmc_adaptor

MANAGEMENT_DIR       := management
MANAGEMENT_SRC_FILES := $(shell find $(MANAGEMENT_DIR) -maxdepth 1 -name "*.c")

MONITOR_DIR       := monitor
MONITOR_SRC_FILES := $(shell find $(MONITOR_DIR) -name "*.c")

SYNCE4L_FILE := synce4l.c

INCS := \
	$(PROJ_DIR) \
	$(COMMON_DIR) \
	$(CONTROL_DIR) \
	$(DEVICE_ADAPTOR_DIR) \
	$(CLOCKMATRIX_API_INC_DIR) \
	$(I2C_DRIVER_DIR) \
	$(ESMC_ADAPTOR_DIR) \
	$(ESMC_STACK_DIR) \
	$(MANAGEMENT_DIR) \
	$(MONITOR_DIR)

PREFIXED_INCS := $(addprefix -I,$(INCS))

DEFINES := \
	SYNCE4L_DEBUG_MODE=$(SYNCE4L_DEBUG_MODE) \
	'DEVICE_DRIVER_PATH(filename)=<$(CLOCKMATRIX_API_INC_DIR)/filename>' \
	'DEVICE_I2C_DRIVER_PATH(filename)=<$(I2C_DRIVER_DIR)/filename>' \
	SERIAL_ADDRESS_MODE=$(SERIAL_ADDRESS_MODE) \
	IDT_CLOCKMATRIX_HOST_ADDRESS_MODE=$(IDT_CLOCKMATRIX_HOST_ADDRESS_MODE)

PREFIXED_DEFINES := $(addprefix -D,$(DEFINES))

SRC_FILES := \
	$(COMMON_SRC_FILES) \
	$(CONTROL_SRC_FILES) \
	$(DEVICE_SRC_FILES) \
	$(I2C_DRIVER_SRC_FILES) \
	$(ESMC_STACK_SRC_FILES) \
	$(MANAGEMENT_SRC_FILES) \
	$(MONITOR_SRC_FILES) \
	$(SYNCE4L_FILE)

OBJS := $(patsubst %.c,%.o,$(SRC_FILES))
OBJS := $(addprefix $(OBJ_DIR)/,$(OBJS))

SYNCE4L := $(BIN_DIR)/synce4l

SYNCE4L_CLI_OBJ_DIR := $(OBJ_DIR)/management/cli

SYNCE4L_CLI_DEFINES := \
	SYNCE4L_CLI_PRINT=1

SYNCE4L_CLI_PREFIXED_DEFINES := $(addprefix -D,$(SYNCE4L_CLI_DEFINES))

SYNCE4L_CLI_FILE := synce4l_cli.c

SYNCE4L_CLI_SRC_FILES := $(SYNCE4L_CLI_FILE) $(COMMON_DIR)/common.c $(COMMON_DIR)/print.c

SYNCE4L_CLI_OBJS := $(patsubst %.c,%.o,$(SYNCE4L_CLI_SRC_FILES))
SYNCE4L_CLI_OBJS := $(addprefix $(SYNCE4L_CLI_OBJ_DIR)/,$(SYNCE4L_CLI_OBJS))

SYNCE4L_CLI := $(BIN_DIR)/synce4l_cli

CC := $(CROSS_COMPILE)gcc

CFLAGS := \
	$(PREFIXED_DEFINES) \
	-Wall \
	-Werror \
	-Wpedantic \
	-Wextra

ifdef USER_CFLAGS
override CFLAGS += $(USER_CFLAGS)
endif

LDFLAGS := \
	-pthread \
	-lrt

ifeq ($(I2C_DRIVER),ftdi)
override LDFLAGS += \
	$(I2C_DRIVER_DIR)/libMPSSE.a \
	-ldl $(I2C_DRIVER_DIR)/libftd2xx.a
endif

ifdef USER_LDFLAGS
override LDFLAGS += $(USER_LDFLAGS)
endif

SYNCE4L_CLI_CFLAGS := \
	$(SYNCE4L_CLI_PREFIXED_DEFINES) \
	-Wall \
	-Werror \
	-Wpedantic \
	-Wextra

.DEFAULT_GOAL := all

###############
# T A R G E T S
###############

# Target: all
.PHONY: all
all: clean synce4l synce4l-cli

# Target: clean
.PHONY: clean
clean:
	@echo "#################################################"
	@echo "#"
	@echo "# C L E A N I N G   B U I L D   A R T I F A C T S"
	@echo "#"
	@echo "#################################################"
	$(RM) -r $(SYNCE4L)
	$(RM) -r $(SYNCE4L_CLI)
	$(RM) -rf $(OBJ_DIR)
	$(RM) -rf $(SYNCE4L_CLI_OBJ_DIR)
	$(RM) -rf $(PKG_DIR)

# Target: synce4l
.PHONY: synce4l
synce4l: synce4l-header create-dirs $(SYNCE4L)

.PHONY: synce4l-header
synce4l-header:
	@echo "#################################"
	@echo "#"
	@echo "# B U I L D I N G   S Y N C E 4 L"
	@echo "#"
	@echo "#################################"
	@echo "ESMC_STACK: $(ESMC_STACK)"
	@echo "PLATFORM: $(PLATFORM)"
	@echo "I2C_DRIVER: $(I2C_DRIVER)"
	@echo "DEVICE: $(DEVICE)"
	@echo "SERIAL_ADDRESS_MODE: $(SERIAL_ADDRESS_MODE)"
ifeq ($(DEVICE),cm) # >>>> Start ClockMatrix definitions
	@echo "IDT_CLOCKMATRIX_HOST_ADDRESS_MODE: $(IDT_CLOCKMATRIX_HOST_ADDRESS_MODE)"
	@echo "REV_ID: $(REV_ID)"
endif # <<<< End ClockMatrix definitions
	@echo "SYNCE4L_DEBUG_MODE: $(SYNCE4L_DEBUG_MODE)"

.PHONY: create-dirs
create-dirs:
	mkdir -p $(BIN_DIR)
	mkdir -p $(OBJ_DIR)
	mkdir -p $(SYNCE4L_CLI_OBJ_DIR)

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) \
		$(PREFIXED_INCS) \
		$(CFLAGS) \
		-o $@ \
		-c $^

$(SYNCE4L): $(OBJS)
	$(CC) \
		-o $@ \
		$^ \
		$(LDFLAGS)

# Target: synce4l-cli
.PHONY: synce4l-cli
synce4l-cli: synce4l-cli-header create-dirs $(SYNCE4L_CLI)

.PHONY: synce4l-cli-header
synce4l-cli-header:
	@echo "#########################################"
	@echo "#"
	@echo "# B U I L D I N G   S Y N C E 4 L   C L I"
	@echo "#"
	@echo "#########################################"

$(SYNCE4L_CLI_OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) \
		$(PREFIXED_INCS) \
		$(SYNCE4L_CLI_CFLAGS) \
		-o $@ \
		-c $^

$(SYNCE4L_CLI): $(SYNCE4L_CLI_OBJS)
	$(CC) \
		-o $@ \
		$^

# Target: help
.PHONY: help
help:
	@echo "###########################################"
	@echo "#"
	@echo "# S Y N C E 4 L   M A K E F I L E   H E L P"
	@echo "#"
	@echo "###########################################"
	@echo "Makefile for synce4l program"
	@echo "  Makefile targets:"
	@echo "    all            - Clean build artifacts, build synce4l binary executable, and build synce4l-cli binary executable"
	@echo "    clean          - Clean build artifacts"
	@echo "    help           - Display Makefile commands"
	@echo "    synce4l        - Build synce4l binary executable"
	@echo "    synce4l-cli    - Build synce4l-cli binary executable"
	@echo "  Makefile command line variables:"
	@echo "    CROSS_COMPILE  - Cross-compiler"
	@echo "                       e.g. Compile for ARM64: CROSS_COMPILE=/usr/bin/aarch64-linux-gnu-"
	@echo "    USER_CFLAGS    - User-defined compiler flag(s)"
	@echo "                       e.g. Compile with C99 standard: USER_CFLAGS=-std=c99"
	@echo "                       e.g. Enable debug mode: USER_CFLAGS=-DSYNCE4L_DEBUG_MODE"
	@echo "                       e.g. Compile with C99 standard and enable debug mode: USER_CFLAGS=\"-std=c99 -DSYNCE4L_DEBUG_MODE\""
	@echo "    USER_LDFLAGS   - User-defined linker flag(s)"
	@echo "                       e.g. Link C math library: USER_LDFLAGS=-lm"
	@echo "                       e.g. Link C thread library: USER_LDFLAGS=-lpthread"
	@echo "                       e.g. Link C math and thread libraries: USER_LDFLAGS=\"-lm -lpthread\""

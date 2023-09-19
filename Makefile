#####################################################################################################################
# @file Makefile
# @note Copyright (C) [2021-2023] Renesas Electronics Corporation and/or its affiliates
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
# Release Tag: 2-0-2
# Pipeline ID: 233453
# Commit Hash: 548f9660
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
$(warning PLATFORM is not defined)
PLATFORM := amd64
$(warning Setting PLATFORM to $(PLATFORM) (default)...)
endif

ifndef CROSS_COMPILE
$(warning CROSS_COMPILE is not defined)
endif
ifeq ($(PLATFORM),arm64)
override CROSS_COMPILE += /usr/bin/aarch64-linux-gnu-
$(warning Setting CROSS_COMPILE to $(CROSS_COMPILE))
endif

# DEVICE:
#  DEVICE := generic --> Generic Device
#  DEVICE := rsmu    --> RSMU
ifndef DEVICE
$(warning DEVICE is not defined)
DEVICE := generic
$(warning Setting DEVICE to $(DEVICE) (default)...)
endif
ifndef SYNCED_DEBUG_MODE
$(warning SYNCED_DEBUG_MODE is not defined (SYNCED_DEBUG_MODE must be either 0 or 1))
override SYNCED_DEBUG_MODE := 0
$(warning Setting SYNCED_DEBUG_MODE to $(SYNCED_DEBUG_MODE) (default)...)
endif

PROJ_DIR  := .
BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin
OBJ_DIR   := $(BUILD_DIR)/obj
PKG_DIR   := pkg

CFG_DIR   := cfg
CFG_FILES := $(shell find $(CFG_DIR) -name "*.cfg")
TCS_FILES := $(shell find $(CFG_DIR) -name "*.tcs")
RBS_FILES := $(shell find $(CFG_DIR) -name "*.rbs")

LICENSE_FILE := COPYING

MAKEFILE := $(MAKEFILE_LIST)

README_FILE := README.md

COMMON_DIR       := common
COMMON_SRC_FILES := $(shell find $(COMMON_DIR) -name "*.c")

CONTROL_DIR       := control
CONTROL_SRC_FILES := $(shell find $(CONTROL_DIR) -name "*.c")

DEVICE_ROOT_DIR          := device
DEVICE_DIR               := $(DEVICE_ROOT_DIR)/$(DEVICE)
DEVICE_ADAPTOR_DIR       := $(DEVICE_ROOT_DIR)/device_adaptor
DEVICE_GENERIC_DIR       := $(DEVICE_ROOT_DIR)/generic
DEVICE_RSMU_DIR          := $(DEVICE_ROOT_DIR)/rsmu
DEVICE_ADAPTOR_SRC_FILES := $(shell find $(DEVICE_ADAPTOR_DIR) -name "*.c")
DEVICE_GENERIC_SRC_FILES := $(shell find $(DEVICE_GENERIC_DIR) -name "*.c")
DEVICE_RSMU_SRC_FILES    := $(shell find $(DEVICE_RSMU_DIR) -name "*.c")
DEVICE_SRC_FILES         := \
	$(DEVICE_ADAPTOR_SRC_FILES) \
	$(DEVICE_GENERIC_SRC_FILES) \
	$(DEVICE_RSMU_SRC_FILES)

ESMC_DIR             := esmc
ESMC_STACK_DIR       := $(ESMC_DIR)/$(ESMC_STACK)
ESMC_ADAPTOR_DIR     := esmc/esmc_adaptor
ESMC_STACK_SRC_FILES := $(shell find $(ESMC_STACK_DIR) -name "*.c")

MANAGEMENT_DIR       := management
MANAGEMENT_SRC_FILES := $(shell find $(MANAGEMENT_DIR) -maxdepth 1 -name "*.c")

MONITOR_DIR       := monitor
MONITOR_SRC_FILES := $(shell find $(MONITOR_DIR) -name "*.c")

SYNCED_FILE := synced.c

INCS := \
	$(PROJ_DIR) \
	$(COMMON_DIR) \
	$(CONTROL_DIR) \
	$(DEVICE_DIR) \
	$(DEVICE_ADAPTOR_DIR) \
	$(DEVICE_GENERIC_DIR) \
	$(DEVICE_RSMU_DIR) \
	$(ESMC_ADAPTOR_DIR) \
	$(ESMC_STACK_DIR) \
	$(MANAGEMENT_DIR) \
	$(MONITOR_DIR)

PREFIXED_INCS := $(addprefix -I,$(INCS))

DEFINES := \
	DEVICE=$(DEVICE) \
	SYNCED_DEBUG_MODE=$(SYNCED_DEBUG_MODE)

PREFIXED_DEFINES := $(addprefix -D,$(DEFINES))

SRC_FILES := \
	$(COMMON_SRC_FILES) \
	$(CONTROL_SRC_FILES) \
	$(DEVICE_SRC_FILES) \
	$(ESMC_STACK_SRC_FILES) \
	$(MANAGEMENT_SRC_FILES) \
	$(MONITOR_SRC_FILES) \
	$(SYNCED_FILE)

OBJS := $(patsubst %.c,%.o,$(SRC_FILES))
OBJS := $(addprefix $(OBJ_DIR)/,$(OBJS))

SYNCED := $(BIN_DIR)/synced

SYNCED_CLI_OBJ_DIR := $(OBJ_DIR)/management/cli

SYNCED_CLI_DEFINES := \
	DEVICE=$(DEVICE)

SYNCED_CLI_PREFIXED_DEFINES := $(addprefix -D,$(SYNCED_CLI_DEFINES))

SYNCED_CLI_FILE := synced_cli.c

SYNCED_CLI_SRC_FILES := $(SYNCED_CLI_FILE) $(COMMON_DIR)/common.c $(COMMON_DIR)/print.c

SYNCED_CLI_OBJS := $(patsubst %.c,%.o,$(SYNCED_CLI_SRC_FILES))
SYNCED_CLI_OBJS := $(addprefix $(SYNCED_CLI_OBJ_DIR)/,$(SYNCED_CLI_OBJS))

SYNCED_CLI := $(BIN_DIR)/synced_cli

CC := $(CROSS_COMPILE)gcc

CFLAGS := \
	$(PREFIXED_DEFINES) \
	-Wall \
	-Werror \
	-Wpedantic \
	-Wextra

ifdef USER_CFLAGS
override CFLAGS += \
	$(USER_CFLAGS)
endif

LDFLAGS := \
	-pthread \
	-lrt

ifdef USER_LDFLAGS
override LDFLAGS += \
	$(USER_LDFLAGS)
endif

SYNCED_CLI_CFLAGS := \
	$(SYNCED_CLI_PREFIXED_DEFINES) \
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
all: clean synced synced-cli

# Target: clean
.PHONY: clean
clean:
	@echo "#################################################"
	@echo "#"
	@echo "# C L E A N I N G   B U I L D   A R T I F A C T S"
	@echo "#"
	@echo "#################################################"
	$(RM) -r $(SYNCED)
	$(RM) -r $(SYNCED_CLI)
	$(RM) -rf $(OBJ_DIR)
	$(RM) -rf $(SYNCED_CLI_OBJ_DIR)
	$(RM) -rf $(PKG_DIR)

# Target: synced
.PHONY: synced
synced: synced-header create-dirs $(SYNCED)

.PHONY: synced-header
synced-header:
	@echo "###############################"
	@echo "#"
	@echo "# B U I L D I N G   S Y N C E D"
	@echo "#"
	@echo "###############################"
	@echo "ESMC_STACK: $(ESMC_STACK)"
	@echo "PLATFORM: $(PLATFORM)"
	@echo "CROSS_COMPILE: $(CROSS_COMPILE)"
	@echo "DEVICE: $(DEVICE)"
	@echo "SYNCED_DEBUG_MODE: $(SYNCED_DEBUG_MODE)"

.PHONY: create-dirs
create-dirs:
	mkdir -p $(BIN_DIR)
	mkdir -p $(OBJ_DIR)
	mkdir -p $(SYNCED_CLI_OBJ_DIR)

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) \
		$(PREFIXED_INCS) \
		$(CFLAGS) \
		-o $@ \
		-c $^

$(SYNCED): $(OBJS)
	$(CC) \
		-o $@ \
		$^ \
		$(LDFLAGS)

# Target: synced-cli
.PHONY: synced-cli
synced-cli: synced-cli-header create-dirs $(SYNCED_CLI)

.PHONY: synced-cli-header
synced-cli-header:
	@echo "#######################################"
	@echo "#"
	@echo "# B U I L D I N G   S Y N C E D   C L I"
	@echo "#"
	@echo "#######################################"

$(SYNCED_CLI_OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) \
		$(PREFIXED_INCS) \
		$(SYNCED_CLI_CFLAGS) \
		-o $@ \
		-c $^

$(SYNCED_CLI): $(SYNCED_CLI_OBJS)
	$(CC) \
		-o $@ \
		$^

# Target: help
.PHONY: help
help:
	@echo "#########################################"
	@echo "#"
	@echo "# S Y N C E D   M A K E F I L E   H E L P"
	@echo "#"
	@echo "#########################################"
	@echo "Makefile for synced program"
	@echo "  Makefile targets:"
	@echo "    all               - Clean build artifacts, build synced binary executable, and build synced_cli binary executable"
	@echo "    clean             - Clean build artifacts"
	@echo "    help              - Display Makefile commands"
	@echo "    synced            - Build synced binary executable"
	@echo "    synced_cli        - Build synced_cli binary executable"
	@echo "  Makefile command line variables:"
	@echo "    ESMC_STACK        - ESMC stack type"
	@echo "                          e.g. Use Renesas ESMC stack: ESMC_STACK=renesas"
	@echo "    PLATFORM          - Platform type"
	@echo "                          e.g. Target AMD64 platform: ESMC_STACK=amd64"
	@echo "                          e.g. Target ARM64 platform: ESMC_STACK=arm64"
	@echo "    CROSS_COMPILE     - Cross-compiler"
	@echo "                          e.g. Compile for ARM64: CROSS_COMPILE=/usr/bin/aarch64-linux-gnu-"
	@echo "    DEVICE            - Device"
	@echo "                          e.g. Employ generic device: DEVICE=generic"
	@echo "                          e.g. Employ RSMU device: DEVICE=rsmu"
	@echo "    SYNCED_DEBUG_MODE - Debug mode enable"
	@echo "                          e.g. Enable debug mode: SYNCED_DEBUG_MODE=1"
	@echo "                          e.g. Disabled debug mode: SYNCED_DEBUG_MODE=0"
	@echo "    USER_CFLAGS       - User-defined compiler flag(s)"
	@echo "                          e.g. Compile with C99 standard: USER_CFLAGS=-std=c99"
	@echo "                          e.g. Enable debug mode: USER_CFLAGS=-DSYNCED_DEBUG_MODE"
	@echo "                          e.g. Compile with C99 standard and enable debug mode: USER_CFLAGS=\"-std=c99 -DSYNCED_DEBUG_MODE\""
	@echo "    USER_LDFLAGS      - User-defined linker flag(s)"
	@echo "                          e.g. Link C math library: USER_LDFLAGS=-lm"
	@echo "                          e.g. Link C thread library: USER_LDFLAGS=-lpthread"
	@echo "                          e.g. Link C math and thread libraries: USER_LDFLAGS=\"-lm -lpthread\""

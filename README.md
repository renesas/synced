@file README.md
<br>@note Copyright (C) [2021-2024] `Renesas Electronics Corporation` and/or its affiliates
<br>This program is free software; you can redistribute it and/or modify
<br>it under the terms of the GNU General Public License version 2, as published by
<br>the Free Software Foundation.
<br>
<br>This program is distributed in the hope that it will be useful,
<br>but WITHOUT ANY WARRANTY; without even the implied warranty of
<br>MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
<br>GNU General Public License for more details.
<br>
<br>You should have received a copy of the GNU General Public License along
<br>with this program; if not, write to the Free Software Foundation, Inc.,
<br>51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***
Release Tag: 2-0-5
<br>Pipeline ID: 310964
<br>Commit Hash: b166f770
***

# `synced` 2-0-5 README

`synced` is a user space Synchronous Ethernet (Sync-E) stack for the Linux operating system.

This README is written in Markdown. Although most editors can be used to view it, it is best viewed
when using a Markdown-compatible editor.

`synced` is copyrighted by `Renesas Electronics Corporation` and is licensed under the GNU General
Public License. See the file COPYING for the license terms.

`synced` can be downloaded from https://github.com/renesas/synced.

Contact IDT-Support-sync@lm.renesas.com to get support for `synced`.

---

## Table of Contents

- [1 Overview](#1_overview)
- [2 Modules](#2_modules)
- [3 Makefile](#3_makefile)
- [4 Configuration](#4_configuration)
- [5 Management API](#5_management_api)
- [6 `synced_cli`](#6_synced_cli)
- [7 `pcm4l`](#7_pcm4l)

---

<a name="1_overview"></a>
## 1. Overview

`synced` facilitates Sync-E according to the ITU-T G.8264 (08/2017) Amd. 1 (03/2018) and ITU-T
G.781 (04/2020) Amd. 1 (11/2022) standards.

Accompanying `synced` is `synced_cli`, which is a command-line interface (CLI) that lets the user
dynamically manage `synced`. `synced` interfaces with `synced_cli` via a TCP/IP socket.

`synced` can also interface with `Renesas Electronics Corporation PTP Clock Manager for Linux`
(`pcm4l`), a high-performance clock recovery solution for packet-based frequency and phase
synchronization, which can be downloaded from https://www.renesas.com. `synced` interfaces with
`pcm4l` via a TCP/IP socket as well.

`synced` consists of the following modules:

 1. **Configuration Module**
 2. **Control Module**
 3. **Device Module**
 4. **ESMC Module**
 5. **Management Module**
 6. **Monitor Module**

`synced` defines four types of ports:

 1. **Sync-E Clock Port**
     - A **Sync-E Clock Port** is associated with an actual device input. It must be assigned
       a clock index, a priority, and an initial quality level (QL).
 2. **Sync-E Monitoring Port**
     - A **Sync-E Monitoring Port**  is not associated with a device input and is only used to
       monitor the QL of its associated Sync-E clock, i.e., it is not considered in the clock
       selection process. It can be converted into a **Sync-E Clock Port** using the **Management
       API**. It must be assigned a priority and an initial QL.
 3. **External Clock Port**
     - A **External Clock Port**  is associated with an actual device input but is not
       Sync-E-capable, i.e., it does not transmit or receive ESMC PDUs. Its QL can be dynamically
       set using the **Management API**. It must be assigned a clock index, a priority, and an
       initial QL.
 4. **Sync-E TX Only Port**
    - A **Sync-E TX Only Port**  is not RX-capable. It is only used to advertise the current
      QL, does not participate in the clock selection process, and has an optional TX bundle
      number parameter. It must not be assigned any other parameters.

The **Control Module** manages the sync table and, thus, refers to a port as a sync. The number of
syncs is limited by the compile-time macro **MAX_NUM_OF_SYNC_ENTRIES**, defined in the
common/common.h header file and is set to 64 by default. The user may change this value, but the
new value cannot exceed 64.

**Sync-E Clock Ports**, **Sync-E Monitoring Ports**, and **Sync-E TX Only Ports** are ESMC-capable,
i.e., they can execute the ESMC protocol and send and/or receive ESMC protocol data units (PDUs).
An **External Clock Port** is not ESMC-capable. The number of ESMC-capable ports is limited by the
compile-time macro **ESMC_MAX_NUM_OF_PORTS**, defined in the esmc/esmc_adaptor/esmc_adaptor.h header
file and set to 32 by default. The user may change this value, but it cannot exceed the value of
the compile-time macro **MAX_NUM_OF_SYNC_ENTRIES**.

`synced` supports a mode called **No QL Mode**. If enabled, the clock selection process is based on
priority. However, only valid clocks participate in the selection process. For example,
if a **Sync-E Clock Port**  has a port link down or RX timeout condition, it will be excluded from
the selection process regardless of its assigned priority.

When there are no valid Sync-E clocks, the device selects the local oscillator (LO)
as the best clock. In this case, the current will be equal to the configured LO
QL.

`synced` provides a timer called **Holdover Timer** to facilitate a gradual transition
from the superior QL associated with a valid Sync-E clock to the QL of the LO when
the Sync-E clock becomes invalid.

<a name="2_modules"></a>
## 2. Modules

### 2.1 Configuration
The **Configuration Module** enables static configuration of `synced` by loading a configuration
file.

### 2.2 Control
The **Control Module** selects the best Sync-E clock for the device through a clock selection
algorithm. It also manages the following TX and RX event callbacks:

 - TX events
   - Port link up (E_esmc_event_type_port_link_up)
   - Port link down (E_esmc_event_type_port_link_down)

 - RX events
   - Invalid received QL (E_esmc_event_type_invalid_rx_ql)
   - QL change (E_esmc_event_type_ql_change)
   - RX timeout (E_esmc_event_type_rx_timeout)
   - Port link up (E_esmc_event_type_port_link_up)
   - Port link down (E_esmc_event_type_port_link_down)
   - Immediate timing loop(E_esmc_event_type_immediate_timing_loop)
   - Originator clock timing loop (E_esmc_event_type_originator_timing_loop)

### 2.3 Device
The **Device Module** manages the timing device. By default, `synced` is built to target a generic
device. However, `synced` can also be built to target a `Renesas Synchronization Management Unit`
(RSMU) device. An RSMU device is a character device that can communicate with a variety of `Renesas
Electronics Corporation timing devices, such as the `Renesas Electronics Corporation ClockMatrix`
timing device. The RSMU driver can be downloaded from
https://github.com/renesas/linux-ptp-driver-package. Contact IDT-Support-sync@lm.renesas.com for
details on employing an RSMU device.

`synced` requires the device to support an automatic reference priority table.

The **Device Module** can support different devices by way of the device adaptor.
The device adaptor supports callbacks that execute device-specific routines.

The device adaptor supports the following callbacks:

 - Initialize device
   - **device_adaptor_call_init_device_cb()**
 - Get the current clock index of Sync-E DPLL
   - **device_adaptor_call_get_current_clk_idx_cb()**
 - Set Sync-E DPLL clock priorities
   - **device_adaptor_call_set_clock_priorities_cb()**
 - Get reference monitor status of the specified clock
   - **device_adaptor_call_get_reference_monitor_status_cb()**
 - Get Sync-E DPLL state
   - **device_adaptor_call_get_synce_dpll_state_cb()**
 - Deinitialize device
   - **device_adaptor_call_deinit_device_cb()**

The Sync-E DPLL can be in the following states:

 - Freerun (E_device_dpll_state_freerun)
 - Lock acquisition-recovery (E_device_dpll_state_lock_acquisition_recovery)
 - Locked (E_device_dpll_state_locked)
 - Holdover (E_device_dpll_state_holdover)

For systems where the received Sync-E clocks are directly connected to the clock inputs of the
device, only the configuration file must be modified. However, `synced` also supports
external multiplexers (muxes) that connect clocks associated with different Ethernet ports to a
single clock input. To use this feature, set the compile-time macro
**ENABLE_EXTERNAL_MUX_CONTROL** to 1 and define the mux using the **g_external_mux_table**
structure in the management/management.c source file. While four muxes are specified in the
example, the code should be changed to match the system hardware. The user must also implement
functions to change the mux under the control of synced, replacing the following comments:

 - /* Call the function to select port 'primary_port_idx' on mux mux_idx here */
 - /* Call the function to select port 'secondary_port_idx' on mux mux_idx here */

### 2.4 ESMC
The **ESMC Module** implements the ESMC protocol at the stack level. ESMC PDUs containing QLs are
transmitted and received by TX-capable and RX-capable Sync-E ports, respectively. The TX-capable
ports are used to advertise the current QL of the device, except for the current best port, which
advertises the QL-DNU and QL-DUS for network options 1 and 2, respectively. The **ESMC Module** can
support different ESMC stacks by way of the ESMC adaptor.

### 2.5 Management
The **Management Module** includes the **Management API**. In addition to Sync-E clocks,
`synced` supports external clocks like GPS, which can be managed using the **Management API**.

### 2.6 Monitor
The **Monitor Module** keeps track of the current QL, current Sync-E DPLL state,
and current clock. It also operates the holdover timer.

<a name="3_makefile"></a>
## 3. Makefile

Makefile commands can only be executed while in the root directory.

 - Enter **make all** to clean the existing build artifacts and build `synced` and
   `synced_cli`.
 - Enter **make clean** to clean the existing `synced` and `synced_cli` build artifacts.
 - Enter **make help** to display the available Makefile commands.
 - Enter **make synced** to only build the `synced` binary executable.
 - Enter **make synced_cli** to only build `synced_cli` binary executable.

To build `synced`, the user must consider the following build arguments:

 - **ESMC_STACK**; default: renesas
 - **PLATFORM**; default: amd64
 - **DEVICE**; default: generic
 - **SYNCED_DEBUG_MODE**; default: 0

When building `synced` via the **make all** or **make synced** commands, the Makefile
will set the build arguments to their default values.

To build `synced` to target an RSMU device, set the build argument **DEVICE** to rsmu.
This assumes the user has already installed the RSMU driver. Below is an example of a build command
to build `synced` to target an RSMU device.

 - Command to build `synced` using the  ESMC stack, targeting a platform with arm64
   architecture, targeting an RSMU, and enabling debug mode:
   - **make synced ESMC_STACK=renesas PLATFORM=arm64 DEVICE=rsmu SYNCED_DEBUG_MODE=1**

Other things to note when running `synced` with an RSMU device:

 - Set the **[device_name]** parameter in the configuration file to the name of the character device
   associated with the RSMU device
 - Load the device configuration file associated with the RSMU device prior to starting
   `synced` using the RSMU driver

As mentioned earlier, by default, `synced` is built to target a generic device, i.e., a device of
the user's choosing, because the build argument **DEVICE** is set to generic by default. Below is an
example of a build command to build `synced` to target a generic device.

 - Command to build `synced` using the  ESMC stack, targeting a platform with amd64
   architecture, targeting a generic device, and disabling debug mode:
   - **make synced ESMC_STACK=renesas PLATFORM=amd64 DEVICE=generic SYNCED_DEBUG_MODE=0**

Other things to note when running `synced` with a generic device:

 - Register the device adaptor callbacks (described in section 2.3) with the appropriate functions,
   as by default, `synced` registers the device adaptor callbacks with template functions, e.g.,
   generic_template_init_device(), generic_template_get_current_clk_idx(), etc.
 - Specify the device configuration file associated with the generic device using the
   **[device_cfg_file]** parameter in the configuration file, as `synced` will load the specified
   device configuration file during startup
 - Replace the function generic_config_device_helper with the appropriate device configuration file
   loader function, as the function generic_config_device_helper() is just a dummy function

The Makefile also supports the **CROSS_COMPILE**, **USER_CFLAGS**, and **USER_LDFLAGS**
build arguments. **CROSS_COMPILE** can be used to set the compiler-compiler.

<a name="4_configuration"></a>
## 4. Configuration

### 4.1 Rules

`synced` will terminate if any of the following configuration rules are broken:

 - Configuration parameters are case-sensitive.
 - Comments can be added to the configuration file but must be placed
   above the line containing the configuration parameter and its corresponding setting.
 - The configuration file supports two types of sections: global and port. Each port
   needs to be configured separately and requires its own port section.
 - Port names must be unique.
 - Clock index assignments must be unique.
 - Priority assignments must be unique.
 - A TX bundle number can be assigned if the Sync-E port is TX-capable.
 - When parameters are not specified, their default values will be applied.

### 4.2 Global Configuration

- Global section:
  - ESMC network option **[net_opt]**
    - Default: 1 (network option 1)
    - Range: 1-3
      - 1: network option 1
      - 2: network option 2
      - 3: network option 3
  - No quality level mode **[no_ql_en]**
    - Default: 0 (disabled)
    - Range: 0-1
  - Sync-E port forced quality level mode enable **[synce_forced_ql_en]**
    - Default: 0 (disabled)
    - Range: 0-1
    - Description:
      - If enabled, forced QL mode support is extended to **Sync-E Clock Ports** and **Sync-E
        Monitoring Ports**
      - If disabled, forced QL support is limited to **External Clock Ports**
  - Local oscillator quality level **[lo_ql]**
    - Default: "FAILED" (QL-FAILED)
    - Range:
      - Network option 1 (highest to lowest)
        - ePRTC
        - PRTC
        - ePRC
        - PRC
        - SSUA
        - SSUB
        - eSEC
        - SEC
        - DNU
        - INV0
        - INV1
        - INV3
        - INV5
        - INV6
        - INV7
        - INV9
        - INV10
        - INV12
        - INV13
        - INV14
        - NSUPP
        - UNC
        - FAILED
      - Network option 2 (highest to lowest)
        - ePRTC
        - PRTC
        - ePRC
        - PRS
        - STU
        - ST2
        - TNC
        - ST3E
        - eEEC
        - ST3
        - SMC
        - ST4
        - PROV
        - DUS
        - INV2
        - INV3
        - INV5
        - INV6
        - INV8
        - INV9
        - INV11
        - NSUPP
        - UNC
        - FAILED
  - Local oscillator priority **[lo_pri]**
    - Default: 255
    - Range: 0-255 (highest to lowest)
  - Maximum message level **[max_msg_lvl]**
    - Default: 7
    - Range: 0-7 (lowest to highest)
      - 0: LOG_EMERG
      - 1: LOG_ALERT
      - 2: LOG_CRIT
      - 3: LOG_ERR
      - 4: LOG_WARNING
      - 5: LOG_NOTICE
      - 6: LOG_INFO
      - 7: LOG_DEBUG
  - stdout enable **[stdout_en]**
    - Default: 1 (enabled)
    - Range: 0-1
  - syslog enable **[syslog_en]**
    - Default: 0 (disabled)
    - Range: 0-1
  - Device configuration file path **[device_cfg_file]**
    - Description:
      - Applicable for generic device
  - Device name **[device_name]**
    - Default: /dev/rsmu1
  - Sync-E DPLL index **[synce_dpll_idx]**
    - Default: 0
    - Range: 0-7
  - Holdover quality level **[holdover_ql]**
    - Default: "FAILED" (QL-FAILED)
    - Range: same as local oscillator quality level
    - Must be equal to or better than the local oscillator quality level
  - Holdover timer in seconds **[holdover_tmr]**
    - Default: 300
    - Range: 0-signed 32-bit integer maximum
  - Hold-off timer in milliseconds **[hoff_tmr]**
    - Default: 300
    - Range: 0-signed 16-bit integer maximum
  - Wait-to-restore timer in seconds **[wtr_tmr]**
    - Default: 300
    - Range: 0-signed 16-bit integer maximum
  - Advanced holdover enable **[advanced_holdover_en]**
    - Default: 0
    - Range: 0-1
    - Description:
      - If enabled, the holdover timer does not expire when the Sync-E DPLL transitions to the lock
        acquisition-recovery state.
  - **`pcm4l` Interface** enable **[pcm4l_if_en]**
    - Default: 0 (disabled)
    - Range: 0-1
    - Description:
      - If enabled, the communication path between `synced` and `pcm4l` will
        be set up (set **[pcm4l_if_ip_addr]** and **[pcm4l_if_port_num]** to the appropriate
        values). This interface is used to send the physical clock category to `pcm4l`
  - **`pcm4l` Interface** IP address **[pcm4l_if_ip_addr]**
    - Example: 127.0.0.1
  - **`pcm4l` Interface** port number **[pcm4l_if_port_num]**
    - Default: 2400
    - Range: 1024-unsigned 16-bit integer maximum
  - **Management Interface** enable **[mng_if_en]**
    - Default: 0 (disabled)
    - Range: 0-1
    - Description:
      - If enabled, the communication path between `synced` and `synced_cli` will
        be set up (set **[mng_if_ip_addr]** and **[mng_if_port_num]** to the appropriate
        values)
  - **Management Interface** IP address **[mng_if_ip_addr]**
    - Example: 127.0.0.2
  - **Management Interface** port number **[mng_if_port_num]**
    - Default: 2400
    - Range: 1024-unsigned 16-bit integer maximum

### 4.3 Port Configuration

- Port section (per port):
  - Name **[name]**
    - Maximum 16 characters in length, including the NULL character
  - Clock index **[clk_idx]**
    - Default: 255 (255 indicates **Sync-E Monitoring Port**)
    - Range: 0-15
    - Description:
      - Do not specify a clock index for **Sync-E Monitoring Ports**, i.e., the clock
        index will be set to 255 internally)
  - Priority **[pri]**
    - Default: 255
    - Range: 0-255 (highest to lowest)
  - TX enable **[tx_en]**
    - Default: 0 (disabled)
    - Range: 0-1
  - RX enable **[rx_en]**
    - Default: 0 (disabled)
    - Range: 0-1
  - Initial QL **[init_ql]**
    - Default: "FAILED" (QL-FAILED)
    - Range: same as local oscillator quality level
  - TX bundle number **[tx_bundle_num]**
    - Default: -1 (disabled/no bundle)
    - Range: -1-255
    - Description:
      - The Sync-E port must be TX-capable (set **[tx_en]** to 1) for this parameter
        to have merit
      - This parameter allows the grouping of TX-capable ports to avoid timing loops,
        i.e., only the best port will advertise the QL, and the others will advertise
        the appropriate do-not-use QL)

<a name="5_management_apis"></a>
## 5. **Management API**

The following generic APIs are supported:

 - Get a sync information list of all the configured **Sync-E Clock Ports**, **Sync-E Monitoring
   Ports**, **External Clock Ports**, and **Sync-E TX Only Ports**
   - **management_get_sync_info_list()**
 - Get the current status (QL, selected port name, selected clock index, Sync-E DPLL
   state and holdover remaining time)
   - **management_get_current_status()**
 - Get the sync information for the specified **Sync-E Clock Port**, **Sync-E Monitoring Port**, or
   **External Clock Port**
   - **management_get_sync_info()**
 - Set the forced QL for specified **Sync-E Clock Port**, **Sync-E Monitoring Port**, or **External
   Clock Port**
   - **management_set_forced_ql()**
 - Clear the forced QL for the specified **Sync-E Clock Port**, **Sync-E Monitoring Port**, or
   **External Clock Port**
   - **management_clear_forced_ql()**
 - Clear the holdover timer
   - **management_clear_holdover_timer()**
 - Clear the wait-to-restore timer for the specified **Sync-E Clock Port** name
   - **management_clear_synce_clk_wtr_timer()**
 - Assign a new **Sync-E Clock Port** for the specified clock index
   - **management_assign_new_synce_clk_port()**
 - Set the priority for the specified port name
   - **management_set_pri()**
 - Set the max message level
   - **management_set_max_msg_level()**

These APIs can be invoked using `synced_cli`.

The following callbacks are supported:

 - Notification for the current QL and selected port name
   - **management_call_notify_current_ql_cb()**
 - Notification for the current QL of the specified **Sync-E Clock Port** or **Sync-E Monitoring
   Port**
   - **management_call_notify_sync_current_ql_cb()**
 - Notification for the current Sync-E DPLL state
   - **management_call_notify_synce_dpll_current_state_cb()**
 - Notification for the current clock state of the specified **Sync-E Clock Port** or **External
   Clock Port**
   - **management_call_notify_sync_current_clk_state_cb()**
 - Notification for the current state of the specified **Sync-E Clock Port**, **Sync-E Monitoring
   Port**, or **External Clock Port**
   - **management_call_notify_sync_current_state_cb()**
 - Notification for the timing loop or invalid received QL alarm
   - **management_call_notify_alarm_cb()**
 - Notification for the `pcm4l` connection status change
   - **management_call_notify_pcm4l_connection_status_cb()**

Unless the user registers their own callback functions, template functions will
be registered in their place.

<a name="6_synced_cli"></a>
## 6. `synced_cli`

As mentioned earlier, `synced_cli` lets the user execute the generic **Management API**
dynamically. `synced` connects to `synced_cli` via a dedicated TCP/IP socket
interface, whose parameters must be specified in the configuration file.

### 6.1 Supported Modes

`synced_cli` supports two modes of operation:
 1. Interactive mode
 2. Command-line mode

`synced_cli` supports the following **Management API** commands:

	- [0: Get sync info list (get_sync_info_list)
	- [1]: Get current status (get_current_status)
	- [2]: Get sync info (get_sync_info)
	- [3]: Set forced QL (set_forced_ql)
	- [4]: Clear forced QL (clear_forced_ql)
	- [5]: Clear holdover timer (clear_holdover_timer)
	- [6]: Clear Sync-E clock wait-to-restore timer (clear_synce_clk_wtr_timer)
	- [7]: Assign new Sync-E clock port (assign_new_synce_clk_port)
	- [8]: Set priority (set_pri)
	- [9]: Set max message level (set_max_msg_lvl)

- Note 1: In interactive mode, enter the code in the square brackets on the left.
- Note 2: In command-line mode, enter the code in the square brackets on the left or the string in
  parentheses on the right.

### 6.2 Interactive Mode

- `synced_cli` runs continuously until the `exit` command is entered by the user
- `synced_cli` prints a list of the supported commands and the codes and strings associated with
  them, when the `help` is entered by the user
- The user must enter the appropriate **Management API** codes
- Example: synced_cli 127.0.0.2 2400 1
  - 127.0.0.2 specifies the IP address of the **Management Interface** of the `synced` application
  - 2400 specifies the port number of the **Management Interface** of `synced` application,
  - 1 specifies the print flag, which indicates whether to enable `synced_cli` messages

### 6.3 Command-Line Mode
- `synced_cli`executes the specified **Management API** commands (one or more) and then exits
- The maximum number of **Management API** commands that can be specified by the user at once is
  limited by the compile-time macro COMMAND_QUEUE_SIZE, defined in management/cli/synced_cli.c
- Example 1: synced_cli 127.0.0.2 2400 1 -c get_sync_info_list -c clear_forced_ql enp0s3 -h
  - Gets the sync info list. Clears the forced QL for port enp0s3. Prints the usage description.
    (Commands are specified using codes.)
- Example 2: synced_cli 127.0.0.2 2400 1 -c 0 -c 4 eth0. (Commands are specified using strings.)
  - Gets the sync info list. Clears the forced QL for port eth0.

### 6.4 Response/Error Codes

`synced_cli` employs the following response/error codes:

 - Ok (E_management_api_response_ok)
 - Invalid (E_management_api_response_invalid)
 - Failed (E_management_api_response_failed)
 - Not supported (E_management_api_response_not_supported)

<a name="7_pcm4l"></a>
## 7. `pcm4l`

When a change in the current QL of the device occurs, `synced` converts this QL
into a physical clock category and then sends it to `pcm4l`. `synced` connects
to `pcm4l` via a dedicated TCP/IP socket interface, whose parameters must be specified
in the configuration file.

The **`pcm4l` Interface** supports the following mapping between the physical clock category
and QL values:

 - E_physical_clock_category_1:
   - E_esmc_ql_net_opt_1_ePRTC
   - E_esmc_ql_net_opt_1_PRTC
   - E_esmc_ql_net_opt_1_ePRC
   - E_esmc_ql_net_opt_1_PRC
   - E_esmc_ql_net_opt_2_ePRTC
   - E_esmc_ql_net_opt_2_PRTC
   - E_esmc_ql_net_opt_2_ePRC
   - E_esmc_ql_net_opt_2_PRS

 - E_physical_clock_category_2:
   - E_esmc_ql_net_opt_1_SSUA
   - E_esmc_ql_net_opt_2_ST2

 - E_physical_clock_category_3:
   - E_esmc_ql_net_opt_1_SSUB
   - E_esmc_ql_net_opt_2_ST3E

 - E_physical_clock_category_4:
   - E_esmc_ql_net_opt_1_eSEC
   - E_esmc_ql_net_opt_1_SEC
   - E_esmc_ql_net_opt_2_STU
   - E_esmc_ql_net_opt_2_TNC
   - E_esmc_ql_net_opt_2_eEEC
   - E_esmc_ql_net_opt_2_ST3
   - E_esmc_ql_net_opt_2_SMC
   - E_esmc_ql_net_opt_2_ST4
   - E_esmc_ql_net_opt_2_PROV

 - E_physical_clock_category_DNU:
   - E_esmc_ql_net_opt_1_DNU
   - E_esmc_ql_net_opt_2_DUS

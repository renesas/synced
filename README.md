@file README.md
<br>@note Copyright (C) [2021-2022] Renesas Electronics Corporation and/or its affiliates
<br>This program is free software; you can redistribute it and/or modify
<br>it under the terms of the GNU General Public License as published by
<br>the Free Software Foundation; either version 2 of the License, or
<br>(at your option) any later version.
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
Release Tag: 1-0-2
<br>Pipeline ID: 118059
<br>Commit Hash: 5a4424ad
***

# `synce4l` 1-0-2 README

This file is a README for `synce4l`, which is an implementation of Synchronous Ethernet
(Sync-E).
<br>It is written in Markdown, so it is best viewed when using a Markdown-compatible
editor.

`synce4l` is copyrighted by Renesas Electronics Corporation and is licensed under
the GNU General Public License.
<br>See the file COPYING for the license terms.

Email at IDT-Support-sync@lm.renesas.com to get in contact about `synce4l`.

---

## Table of Contents

- [1 Overview](#1_overview)
- [2 Makefile](#2_makefile)
- [3 Configuration](#3_configuration)
- [4 Management API](#4_management_api)
- [5 `synce4l_cli`](#5_synce4l_cli)

---

<a name="1_overview"></a>
## 1. Overview

`synce4l` facilitates Sync-E, according to the ITU-T G.8264 (03/2018) and ITU-T
G.781 (04/2020) standards.

Accompanying `synce4l` is `synce4l_cli`, which is a command-line interface (CLI)
that lets the user dynamically manage `synce4l`.

`synce4l` consists of the following modules:

- **Configuration**
- **Control**
- **Device**
- **ESMC**
- **Management**
- **Monitor**

`synce4l` defines four types of ports:

1. **Sync-E Clock**
2. **Sync-E Monitoring**
3. **External Clock**
4. **Sync-E TX Only**

### Modules

The **Configuration** module enables static configuration of `synce4l` by loading
a configuration file.

The **Control** module selects the best Sync-E clock for the device through a selection
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

The **Device** module manages the timing device (e.g. `Renesas Electronics Corporation
ClockMatrix` device). To use a `Renesas Electronics Corporation ClockMatrix` device,
the user must download `ClockMatrix API` from https://www.renesas.com/us/en. Once
downloaded, the `ClockMatrix API` folder must be copied inside the device folder,
which is in the root directory. Once copied, the `ClockMatrix API` folder must be
renamed to *clockmatrix-api*. This module loads the specified device configuration
file. `synce4l` supports a single device (i.e. a single DPLL).

The **ESMC** module implements the ESMC protocol at the stack level. ESMC protocol
data units (PDUs) containing quality levels (QLs) are transmitted and received by
TX-capable and RX-capable Sync-E ports, respectively. The TX-capable ports are used
to advertise the current QL of the device, except for the current best port which
advertises the QL-DNU and QL-DUS for network options 1 and 2, respectively.

The **Management** module contains **Management API**. In addition to Sync-E clocks,
`synce4l` supports external clocks like GPS, which can be managed by the said APIs.

The **Monitor** module keeps track of the current QL, current Sync-E DPLL state,
and current clock. It also operates the holdover timer.

### Ports

A **Sync-E Clock** port is associated with an actual device input. It must be assigned
a clock index, priority, and initial QL.

A **Sync-E Monitoring** port is not associated with any device input and is used
to only monitor the QL of its associated Sync-E clock (i.e. it is not considered
in the selection process). It can be switched into a **Sync-E Clock** port using
the **Management API** and must be assigned a priority and initial QL.

A **External Clock** port is associated with an actual device input, but is not
Sync-E capable (i.e. it does not transmit or receive ESMC PDUs). Its QL can be dynamically
set using **Management API** and must be assigned a clock index, priority, and initial
QL.

A **Sync-E TX Only** port is not RX-capable. It is only used to advertise the current
QL, does not participate in the selection process, and has an optional TX bundle
number parameter (i.e. no other parameters should be configured).

### Other

`synce4l` supports a mode called **No QL Mode**. If enabled, the selection process
is based on priority. However, only valid clocks participate in the selection, meaning
if a **Sync-E Clock** port has a port link down or RX timeout condition, then it
will be excluded from selection process regardless of its assigned priority.

When there are no valid Sync-E clocks, the device selects the local oscillator (LO)
as the best clock. In this case, the current will be equal to the configured LO
QL.

`synce4l` provides a timer called **Holdover Timer** to facilitate a gradual transition
from the superior QL associated with a valid Sync-E clock to the QL of the LO, when
the aforementioned Sync-E clock becomes invalid.

<a name="2_makefile"></a>
## 2. Makefile

Makefile commands can only be executed while in the root directory.

- Enter **make all** to clean the existing build artifacts and build `synce4l` and
`synce4l_cli`.
- Enter **make clean** to clean the existing `synce4l` and `synce4l_cli` build artifacts.
- Enter **make help** to display the available Makefile commands.
- Enter **make synce4l** to build the `synce4l` binary executable only.
- Enter **make synce4l_cli** to build `synce4l_cli` binary executable only.

To build `synce4l`, the user must consider the following build arguments:

- **ESMC_STACK**; default: renesas
- **PLATFORM**; default: amd64
- **I2C_DRIVER**; default: rsmu
- **SERIAL_ADDRESS_MODE**; default: 8
- **REV_ID**; default: rev-d-p
- **SYNCE4L_DEBUG_MODE**; default: 0

When building `synce4l` via the **make all** or **make synce4l** commands, the Makefile
will set the build arguments to their default values.

For example, the following command can be used to build `synce4l` using the 
ESMC stack, targeting a platform with arm64 architecture, employing the `RSMU` driver,
using 8-bit I2C slave addressing, targeting the `Renesas Electronics Corporation
ClockMatrix Rev-E` device, and enabling debug mode:

**make synce4l ESMC_STACK=renesas PLATFORM=arm64 I2C_DRIVER=rsmu SERIAL_ADDRESS_MODE=8
REV_ID=rev-e SYNCE4L_DEBUG_MODE=1**

The Makefile also supports the **CROSS_COMPILE**, **USER_CFLAGS**, and **USER_LDFLAGS**
build arguments. **CROSS_COMPILE** can be used to set the compiler-compiler. In
the example above, **CROSS_COMPILE** sets invokes the AARCH64 cross-compiler.

<a name="3_configuration"></a>
## 3. Configuration

### Rules

Configuration parameters are case-sensitive.

Comments can be added to the configuration file, but they must be placed in the line
above the line containing the configuration parameter and its corresponding setting.

The configuration file supports two types of sections: global and port. Each port
needs to be configured separately and requires its own port section.

Port names must be unique.

Clock index assignments must be unique.

Priority assignments must be unique.

A TX bundle number can be assigned if the Sync-E port is TX-capable.

When parameters are not specified, their default values will be applied.

`synce4l` will terminate if the configuration rules are broken.

### Global Configuration

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
      - If enabled, forced QL mode support is extended to Sync-E ports
      - If disabled, forced QL support is limited to external clock ports
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
  - Device configuration file path **[tcs_file]**
  - Sync-E DPLL index **[synce_dpll_idx]**
    - Default: 0
    - Range: 0-7
  - Holdover quality level **[holdover_ql]**
    - Default: "FAILED" (QL-FAILED)
    - Range: same as local oscillator quality level
    - Must be equal to or better than local oscillator quality level
  - Holdover timer in seconds **[holdover_tmr]**
    - Default: 300
    - Range: 0-signed 32-bit integer maximum
  - Hold-off timer in milliseconds **[hoff_tmr]**
    - Default: 300
    - Range: 0-signed 16-bit integer maximum
  - Wait-to-restore timer in seconds **[wtr_tmr]**
    - Default: 300
    - Range: 0-signed 16-bit integer maximum
  - pcm4l interface enable **[pcm4l_if_en]**
    - Default: 0 (disabled)
    - Range: 0-1
    - Description:
      - If enabled, the communication path between `synce4l` and `pcm4l` will
      be set up (set **[pcm4l_if_ip_addr]** and **[pcm4l_if_port_num]** to the appropriate
      values). This interface is used to send the physical clock category to pcm4l
  - pcm4l interface IP address **[pcm4l_if_ip_addr]**
    - Default:
    - Range:
    - Example: 127.0.0.1
  - pcm4l interface port number **[pcm4l_if_port_num]**
    - Default: 2400
    - Range: 1024-unsigned 16-bit integer maximum
  - Management interface enable **[mng_if_en]**
    - Default: 0 (disabled)
    - Range: 0-1
    - Description:
      - If enabled, the communication path between `synce4l` and `synce4l_cli` will
      be set up (set **[mng_if_ip_addr]** and **[mng_if_port_num]** to the appropriate
      values)
  - Management interface IP address **[mng_if_ip_addr]**
    - Default:
    - Range:
    - Example: 127.0.0.2
  - Management interface port number **[mng_if_port_num]**
    - Default: 2400
    - Range: 1024-unsigned 16-bit integer maximum

### Port Configuration

- Port section (per port):
  - Name **[name]**
    - Maximum 16 characters in length, including the NULL character
  - Clock index **[clk_idx]**
    - Default: 255 (255 indicates Sync-E monitoring port)
    - Range: 0-15
    - Description:
      - Do not specify a clock index for Sync-E monitoring ports (i.e. the clock
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
      - This parameter allows the grouping of TX-capable ports to avoid timing loops
      (i.e. only the best port will advertise the QL and the others will advertise
      the appropriate do-not-use QL)

<a name="4_management_apis"></a>
## 4. **Management API**

The following generic APIs are supported:

- Get a sync information list of all the configured Sync-E clock, Sync-E monitoring,
external clock, and Sync-E TX only ports
  - **management_get_sync_info_list()**
- Get the current status (QL, selected port name, selected clock index, Sync-E DPLL
state and holdover remaining time)
  - **management_get_current_status()**
- Get the sync information for the specified Sync-E clock, Sync-E monitoring, or
external clock port
  - **management_get_sync_info()**
- Set the forced QL for specified Sync-E clock, Sync-E monitoring, or external clock
port
  - **management_set_forced_ql()**
- Clear the forced QL for the specified Sync-E clock, Sync-E monitoring, or external
clock port
  - **management_clear_forced_ql()**
- Clear the holdover timer
  - **management_clear_holdover_timer()**
- Clear the wait-to-restore timer for specified Sync-E clock port name
  - **management_clear_synce_clk_wtr_timer()**
- Assign a new Sync-E clock port for the specified clock index
  - **management_assign_new_synce_clk_port()**
- Set the priority for the specified port name
  - **management_set_pri()**
- Set the max message level
  - **management_set_max_msg_level()**

These APIs can be executed using `synce4l_cli`.

The following callbacks are supported:

- Notification for the current QL
  - **management_call_notify_current_ql_cb()**
- Notification for the current QL of the specified Sync-E clock or monitoring port
  - **management_call_notify_sync_current_ql_cb()**
- Notification for the current Sync-E DPLL state
  - **management_call_notify_synce_dpll_current_state_cb()**
- Notification for the current clock state of the specified Sync-E clock or external
clock port
  - **management_call_notify_sync_current_clk_state_cb()**
- Notification for the current state of the specified Sync-E clock, Sync-E monitoring,
or external clock port
  - **management_call_notify_sync_current_state_cb()**
- Notification for the timing loop or invalid received QL alarm
  - **management_call_notify_alarm_cb()**
- Notification for the pcm4l connection status change
  - **management_call_notify_pcm4l_connection_status_cb()**

Unless the user registers their own callback functions, template functions will
be registered in their place.

<a name="5_synce4l_cli"></a>
## 5. `synce4l_cli`

As mentioned above, `synce4l_cli` lets the user execute the generic **Management API**
dynamically.

`synce4l_cli` employs the following response/error codes:

- Ok (E_management_api_response_ok)
- Invalid (E_management_api_response_invalid)
- Failed (E_management_api_response_failed)
- Not supported (E_management_api_response_not_supported)

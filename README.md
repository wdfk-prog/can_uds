[中文](README_ZN.md)

## 1. Project Overview

An implementation of the ISO 14229 (Unified Diagnostic Services, UDS) diagnostic tool based on the RT-Thread operating system, comprising a server and a client. The server runs on an embedded device and communicates with the client via the CAN bus, implementing various UDS diagnostic services to facilitate remote diagnosis and control of embedded devices.

This project aims to provide RT-Thread users with a complete UDS diagnostic solution that can be used directly or serve as a reference for learning the UDS protocol and developing custom diagnostic functions.

### Main Features
- Based on the RT-Thread operating system, fully utilizing its multitasking and device driver framework.
- Implements commonly used UDS diagnostic services to meet most diagnostic requirements.
- Provides a cross-platform Linux client for convenient interaction with the server.
- Modular design, making it easy to extend with new UDS services.
- Includes rich sample code to help users get started quickly.

### Application Scenarios
- Automotive ECU diagnostics
- Remote maintenance of industrial controllers
- Firmware upgrades for embedded devices
- Device status monitoring and troubleshooting

---

## 2. Features

### Server Features
- Multitasking architecture based on RT-Thread, ensuring system stability and real-time performance.
- Complete implementation of the ISO 14229 UDS protocol stack.
- Supports multiple UDS diagnostic services, including session control, security access, parameter read/write, etc.
- Flexible service registration mechanism for easy extension of new features.
- Seamless integration with RT-Thread CAN device drivers.
- Supports CAN bus communication control, enabling/disabling message transmission/reception dynamically.

### Client Features
- Based on the Linux platform, using SocketCAN for CAN communication.
- Provides a command-line interactive interface with command history and auto-completion.
- Implements client-side functionality for UDS services that match the server.
- Supports file upload and download functionality.
- Supports execution of remote console commands.
- Features comprehensive error handling and log display capabilities.

### Communication Features
- CAN bus data transmission protocol based on ISO TP (ISO 15765-2).
- Supports large data block transmission, suitable for scenarios like file transfers.
- Equipped with flow control mechanisms to ensure communication stability.
- Supports both functional addressing and physical addressing communication methods.

---

## 3. Supported UDS Services

This project implements the following UDS services defined by the ISO 14229 standard:

| Service ID | Name                          | Description                            |
|------------|-------------------------------|----------------------------------------|
| 0x10       | Diagnostic Session Control    | Controls diagnostic session mode (Default/Extended/Programming Session) |
| 0x11       | ECU Reset                     | Performs ECU reset operation            |
| 0x22       | Read Data By Identifier       | Reads parameter values corresponding to Data Identifiers |
| 0x27       | Security Access               | Security access service to unlock advanced diagnostic functions |
| 0x28       | Communication Control         | Controls communication behavior (Enables/Disables message transmission/reception) |
| 0x2E       | Write Data By Identifier      | Writes parameter values corresponding to Data Identifiers |
| 0x2F       | Input/Output Control          | Controls the behavior of input/output signals |
| 0x31       | Routine Control               | Controls routine start, stop, and result inquiry |
| 0x34       | Request Download              | Requests downloading data to ECU        |
| 0x36       | Transfer Data                 | Transfers data blocks                   |
| 0x37       | Request Transfer Exit         | Ends data transfer                      |
| 0x38       | Request File Transfer         | Requests file transfer                  |
| 0x3E       | Tester Present                | Keeps tester online                     |

These services cover the core functionalities of the UDS protocol and can meet the requirements of most diagnostic application scenarios.

---

## 4. System Architecture

### Overall Architecture Diagram
```
+------------------+        CAN Bus        +------------------+
|                  | <=================>  |                  |
|  Linux Client    |                      | RT-Thread Server |
|                  |                      |                  |
+------------------+                      +------------------+
       |                                           |
       | SocketCAN                                 | RT-Thread CAN Driver
       |                                           |
       v                                           v
+------------------+                      +------------------+
| UDS Client Stack |                      | UDS Server Stack |
| (iso14229.c)     |                      | (iso14229_rtt.c) |
+------------------+                      +------------------+
       |                                           |
       | Application Layer                         | Application Layer
       v                                           v
+------------------+                      +------------------+
| Client Services  |                      | Server Services  |
| (services/)      |                      | (service/)       |
+------------------+                      +------------------+
```

### Server Architecture
The server is based on the RT-Thread operating system and adopts a layered architecture design:
1. **Hardware Abstraction Layer**: RT-Thread CAN device driver
2. **Transport Layer**: ISO TP protocol implementation
3. **Protocol Layer**: ISO 14229 core protocol stack (`[iso14229_rtt.c]`)
4. **Service Layer**: Specific implementations of various UDS services (`service/`)
5. **Application Layer**: User-defined application logic (`[rtt_uds_example.c]`)

### Client Architecture
The client is based on the Linux platform and also adopts a layered architecture design:
1. **Hardware Abstraction Layer**: SocketCAN interface
2. **Transport Layer**: ISO TP protocol implementation
3. **Protocol Layer**: ISO 14229 core protocol stack (`[iso14229.c]`)
4. **Service Layer**: Client implementations of various UDS services (`services/`)
5. **Application Layer**: Command-line interactive interface and user command processing (`core/`, `[main.c]`)

---

## 5. Code Structure

### Server Code Structure
```
server_demo/
├── examples/                    # Sample Applications
│   └── rtt_uds_example.c       # RT-Thread UDS Server Example
├── service/                     # UDS Service Implementations
│   ├── rtt_uds_service.h       # Service Definition Macros and APIs
│   ├── service_0x10_session.c  # 0x10 Diagnostic Session Control Service
│   ├── service_0x11_reset.c    # 0x11 ECU Reset Service
│   ├── service_0x22_0x2E_param.c # 0x22/0x2E Parameter Read/Write Service
│   ├── service_0x27_security.c # 0x27 Security Access Service
│   ├── service_0x28_comm.c     # 0x28 Communication Control Service
│   ├── service_0x2F_io.c       # 0x2F Input/Output Control Service
│   ├── service_0x31_console.c  # 0x31 Remote Console Service
│   └── service_0x36_0x37_0x38_file.c # 0x36/0x37/0x38 File Transfer Service
├── iso14229_rtt.c              # UDS Protocol Core Implementation (RT-Thread Version)
├── iso14229_rtt.h              # UDS Protocol Core Header File
└── rtt_uds_config.h            # RT-Thread Platform Configuration File
```

### Client Code Structure
```
client_demo/
├── core/                        # Core Modules
│   ├── client.h                # Client Common Header File
│   ├── client_config.c/h       # Client Configuration Management
│   ├── client_shell.c/h        # Command-Line Interactive Interface
│   ├── cmd_registry.c/h        # Command Registration Management
│   ├── response_registry.c/h   # Response Registration Management
│   └── uds_context.c/h         # UDS Context Management
├── services/                    # UDS Service Client Implementations
│   ├── client_0x10_session.c   # 0x10 Diagnostic Session Control Client
│   ├── client_0x11_reset.c     # 0x11 ECU Reset Client
│   ├── client_0x22_0x2E_param.c # 0x22/0x2E Parameter Read/Write Client
│   ├── client_0x27_security.c  # 0x27 Security Access Client
│   ├── client_0x28_comm.c      # 0x28 Communication Control Client
│   ├── client_0x2F_io.c        # 0x2F Input/Output Control Client
│   ├── client_0x31_console.c   # 0x31 Remote Console Client
│   └── client_0x36_0x37_0x38_file.c # 0x36/0x37/0x38 File Transfer Client
├── utils/                       # Utility Modules
│   ├── linenoise.c/h           # Command-Line Editing Library
│   └── utils.c/h               # General Utility Functions
├── main.c                      # Main Program Entry Point
└── Makefile                    # Build Script
```

---

## 6. Hardware Requirements

### Server Side
- An embedded development board running the RT-Thread operating system.
- At least one hardware interface with a CAN controller.
- Connected to a CAN transceiver (e.g., TJA1050, SN65HVD230).
- GPIO pins for the LED control example (optional).

### Client Side
- A host machine (physical or virtual) running the Linux operating system.
- A SocketCAN-compatible CAN interface adapter.
  - USB-to-CAN adapter (e.g., USBCAN-I, USBCAN-II).
  - PCIe CAN interface card.
  - Development board with a CAN controller (e.g., Raspberry Pi with MCP2515).
- CAN bus connection cables and termination resistors.

---

## 7. Getting Started

### Server Setup

1. **Configuration**
    - Enable the package in RT-Thread's `menuconfig`:

    ```sh
    RT-Thread online packages  --->
        peripherals packages  --->
            [*] Enable iso14229 (UDS) library  --->
                (32) Event Dispatch Table Size
                [*] Enable UDS server example application
    ```

2. **Library Integration**
   - Add the source files from the `server_demo` directory to your RT-Thread project.
   - Ensure CAN device driver support is enabled in your project configuration.

3. **Compilation and Flashing**
   - Compile the project using the toolchain supported by RT-Thread.
   - Flash the generated firmware to the target hardware.

4. **Running the Example**
   - Connect to the device console via a serial terminal.
   - Start the UDS server example:
     ```
     msh />uds_example start can1
     ```

### Client Setup

1. **Configure SocketCAN**
   - Enable and configure the CAN interface on the Linux host:
     ```bash
     # Load CAN modules (if needed)
     sudo modprobe can
     sudo modprobe can_raw
     sudo modprobe vcan  # For virtual CAN testing

     # Configure a physical CAN interface (e.g., at 1 Mbps)
     sudo ip link set can0 up type can bitrate 1000000

     # Or create a virtual CAN interface for testing
     sudo ip link add dev vcan0 type vcan
     sudo ip link set up vcan0
     ```

2. **Compile the Client**
   - Navigate to the `client_demo` directory.
   - Modify the cross-compilation toolchain path in the [Makefile](file:///home/embedsky/share/iso14229/examples/rtt_server/client_demo/Makefile) (if necessary).
   - Execute the compilation command:
     ```bash
     make                    # Cross-compile (default)
     make NATIVE=1          # Native compilation for testing
     ```

3. **Run the Client**
   - Execute the generated client program:
     ```bash
     ./client -i can0 -s 7E8 -t 7E0  # Use can0 interface
     ./client -i vcan0               # Use virtual CAN interface
     ```

---

## 8. Usage

### Server Usage

Control the UDS server via RT-Thread's MSH command-line interface:

```bash
# Start the UDS server
msh />uds_example start can1

# Stop the UDS server
msh />uds_example stop can1

# View registered services
msh />uds_list
```

Once started, the server will listen for diagnostic requests on the specified CAN interface and provide corresponding service functions based on the configuration.

### Client Usage

The client provides an interactive command-line interface supporting various diagnostic commands:

```bash
# Start the client (uses can1 interface by default)
$ ./client

# Start the client and specify interface and addresses
$ ./client -i vcan0 -s 7E8 -t 7E0

# Common commands after entering the interactive interface:
UDS> help              # Display help information
UDS> session 03        # Switch to extended session
UDS> security 01       # Perform security access (level 01)
UDS> wdbi 0001 01      # Write value 01 to DID 0001
UDS> io 0100 03 01 00 00  # Control IO (Force red LED)
UDS> sy local_file.bin # Upload file to server
UDS> ry remote_file.bin # Download file from server
UDS> rexec ps          # Execute command on server
UDS> cd /              # Change directory on server
UDS> lls               # List local files
UDS> exit              # Exit the client
```

---

## 9. Service Details

### 0x10 Diagnostic Session Control
Controls the ECU's diagnostic session mode. Different session modes have varying security levels and functional access permissions:
- Default Session (0x01): Basic diagnostic functions.
- Programming Session (0x02): Used for firmware flashing.
- Extended Session (0x03): Accesses more diagnostic functions.

### 0x27 Security Access
Protects sensitive diagnostic functions using a seed-key mechanism:
1. Client requests a seed for a specific security level.
2. Server returns a random seed value.
3. Client calculates the key based on an algorithm.
4. Client sends the key for verification.
5. Upon successful verification, the corresponding security level is unlocked.

### 0x2F Input/Output Control
Controls digital input/output signals of the ECU:
- Short Term Adjustment: Temporarily changes the IO state.
- Return Control: Returns IO control authority to the application.
- Freeze Current State: Locks the current IO state.
- Reset To Default: Restores the IO to its default state.

### 0x31 Remote Console
Executes command-line instructions on the server via the UDS service, enabling remote control functionality:
- Execute any MSH command.
- Get the command execution result.
- Supports directory switching and file browsing.

### 0x36/0x37/0x38 File Transfer
Complete file transfer functionality supporting uploads and downloads:
1. 0x38 Request File Transfer: Negotiates file transfer parameters.
2. 0x36 Transfer Data: Transfers file data blocks.
3. 0x37 Request Transfer Exit: Ends the transfer and verifies integrity.

---

## 10. Examples and Logs

### Typical Interaction Log

```sh
# Client Startup
$ ./client -i vcan0
[Config] IF: vcan0 | SA: 0x7E8 | TA: 0x7E0 | FUNC: 0x7DF

[Shell] Interactive Mode Started. Type 'help' or 'exit'.

UDS> 

# Switch to Extended Session
UDS> session 03
[I] (940) Session: Requesting Session Control: 0x03
[+] Switching Session Done.   
[I] (950) Session: Session Switched Successfully (0x03)

# Perform Security Access
UDS> security 01
[I] (1020) Sec: Starting Security Access (Level 0x01)...
[+] Requesting Seed Done.   
[I] (1030) Sec: Seed: 0x12345678 -> Key: 0xB791F3D5
[+] Verifying Key Done.   
[I] (1040) Sec: Security Access Granted!

# Control LED
UDS> io 0100 03 01 00 00
[I] (1120) IO Ctrl: Sending IO Control: DID=0x0100 Param=0x03
[+] Requesting Done.   

# Upload File
UDS> sy test.txt
[*] Uploading 'test.txt' (1024 bytes)...
[+] Requesting Done.   
[=>] 1024/1024 bytes
[+] Finishing Done.   
[+] Upload Complete.

# Execute Remote Command
UDS> rexec ps
[I] (1250) Console: Remote Exec: 'ps'
msh />ps
 thread  pri  status      sp     stack size max used   left tick  error
------ ---- ------- ---------- ----------  ------  ---------- ---
tidle   31  ready   0x00000048 0x00000100    28%   0x0000000b 000
timer    4  suspend 0x00000068 0x00000200    16%   0x00000009 000
main    10  running 0x00000070 0x00000800    11%   0x0000000b 000
uds_sr   2  suspend 0x00000070 0x00001000    13%   0x00000014 000
```

---

## 11. Development Guide

### Adding New UDS Services

1. On the Server:
   - Create a new service implementation file in the `service/` directory.
   - Implement the service handler function.
   - Register the service node with the UDS environment.

2. On the Client:
   - Create a client implementation file in the `services/` directory.
   - Implement the command handler function.
   - Register the command with the command management system.

### Configuration Notes

Key configurations in the client Makefile:
```makefile
# Optimize ISO-TP flow control parameters for improved transfer efficiency
CFLAGS += -DISOTP_FC_BS=0      # Block size set to 0 (No flow control)
CFLAGS += -DISOTP_FC_STMIN=0   # Minimum separation time set to 0
```

## 12. related links
- [iso14229 code](https://github.com/driftregion/iso14229/)
- [iso14229 client demo](https://github.com/wdfk-prog/iso14229/tree/rtt/examples/rtt_server/client_demo)
- [iso14229 rtt software](https://github.com/wdfk-prog/can_uds)

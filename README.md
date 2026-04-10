[中文](README_ZN.md)

# can_uds

`can_uds` is an **RT-Thread-oriented ISO 14229-1 / UDS server-side integration repository** focused on the **ECU / server side**. The repository currently provides:

- a generic UDS / ISO-TP protocol core and headers
- an RT-Thread port layer and runtime environment
- a set of mountable business services under `service/`
- example applications, including a base example and a `0x2A + ULOG` periodic-streaming example

> The repository contents are centered on the RT-Thread server component, and this README has been rewritten to match the code that actually exists today.

---

## 1. Positioning

Typical use cases include:

- MCU / ECU diagnostics
- production-line test, after-sales maintenance, and remote service
- DID-backed parameter access
- `0x2F` I/O control
- periodic data or log streaming through `0x2A`
- remote console execution through `0x31`
- file read/write workflows through `0x34 ~ 0x38`

This is not just a bare protocol core. It is a **practical RT-Thread UDS integration framework**.

---

## 2. Repository layout

```text
can_uds/
├── examples/
├── service/
├── docs/
│   ├── zh/
│   └── en/
├── iso14229.c
├── iso14229.h
├── iso14229_rtt.c
├── iso14229_rtt.h
├── rtt_uds_config.h
├── Kconfig
├── SConscript
├── README.md
├── README_en.md
└── package.json
```

---

## 3. Implemented services

| SID | Service | Implementation file | Notes |
|---|---|---|---|
| 0x10 | Diagnostic Session Control | `service_0x10_session.c` | default / extended / programming sessions |
| 0x11 | ECU Reset | `service_0x11_reset.c` | delayed reset after positive response |
| 0x22 | ReadDataByIdentifier | `service_0x22_0x2E_param.c` | parameter-read adapter |
| 0x27 | Security Access | `service_0x27_security.c` | Seed / Key unlock flow |
| 0x28 | Communication Control | `service_0x28_comm.c` | application RX/TX gating |
| 0x2A | ReadDataByPeriodicIdentifier | `service_0x2A_periodic.c` | provider registration and scheduling |
| 0x2E | WriteDataByIdentifier | `service_0x22_0x2E_param.c` | parameter-write adapter |
| 0x2F | InputOutputControlByIdentifier | `service_0x2F_io.c` | takeover / return-control handling |
| 0x31 | RoutineControl | `service_0x31_console.c` | remote-console routine |
| 0x34/0x36/0x37/0x38 | File-transfer related services | `service_0x36_0x37_0x38_file.c` | file I/O with CRC closure |
| 0x3E | TesterPresent | core | session keep-alive |

---

## 4. Client / Tester Implementation

This repository mainly provides a **UDS server (ECU-side)** implementation.

If you need a corresponding diagnostic client (tester), refer to the resources below:

### Source demo

👉 https://github.com/wdfk-prog/iso14229/tree/rtt/examples/rtt_server/client_demo

This demo is based on Linux + SocketCAN / ISO-TP and can be used for:

- Integration testing with this server
- Verifying UDS services (0x10 / 0x22 / 0x31 / file transfer, etc.)
- Serving as a base for production or debugging tools

### Published client binaries

If you want a ready-to-use published client package, use this release page:

- Release page for Windows and x86-64 client binaries:
  https://github.com/wdfk-prog/iso14229/releases/tag/client_demo-v1.0.0

> Recommendation: use this client_demo during early bring-up and protocol validation. If you need a prebuilt binary, go to the release page above and pick the matching platform package.

## 5. Quick start

1. Enable the package and the services you need in `menuconfig`.
2. Make sure the BSP CAN driver can send and receive frames correctly.
3. Create the runtime environment with `rtt_uds_create()`.
4. Mount services with `rtt_uds_*_service_mount()`.
5. Feed received CAN frames into the stack with `rtt_uds_feed_can_frame()`.

```c
#include "rtt_uds_service.h"

static rtt_uds_env_t *g_env;

RTT_UDS_SEC_SERVICE_DEFINE(security_service, 0x01, 0xA5A5A5A5);
RTT_UDS_IO_SERVICE_DEFINE(io_service);
RTT_UDS_FILE_SERVICE_DEFINE(file_service);
RTT_UDS_0X2A_SERVICE_DEFINE(periodic_service);

static int uds_app_init(void)
{
    rtt_uds_config_t cfg = {0};

    g_env = rtt_uds_create(&cfg);
    if (g_env == RT_NULL)
    {
        return -RT_ERROR;
    }

    rtt_uds_sec_service_mount(g_env, &security_service);
    rtt_uds_io_service_mount(g_env, &io_service);
    rtt_uds_file_service_mount(g_env, &file_service);
    rtt_uds_0x2a_service_mount(g_env, &periodic_service);

    return RT_EOK;
}
```

For a guided setup path, see [`docs/en/quick-start.md`](docs/en/quick-start.md).

---

## 6. Documentation

### API Documentation

The complete API reference (HTML) is published through GitHub Pages:

➡️ https://wdfk-prog.space/can_uds/

### Project Docs (English)

- [Architecture](docs/en/architecture.md)
- [Quick Start](docs/en/quick-start.md)
- [Service Guide](docs/en/service-guide.md)
- [DID Design](docs/en/did-design.md)
- [File Transfer](docs/en/file-transfer.md)
- [Console Service](docs/en/console-service.md)
- [0x2A Periodic Service](docs/en/0x2a-periodic.md)
- [Porting Notes](docs/en/porting-notes.md)

---

## 7. Chinese docs

See [`README_ZN.md`](README_ZN.md) for the Chinese entry, or go directly to [`docs/zh/`](docs/zh/).

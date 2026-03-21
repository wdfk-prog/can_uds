[English](README.md)

# can_uds

`can_uds` 是一个面向 **RT-Thread** 的 **ISO 14229-1 / UDS 服务端集成仓库**，重点在 **ECU / server 侧**。当前仓库提供：

- 通用的 UDS / ISO-TP 协议核心与头文件
- RT-Thread 适配层与运行时环境封装
- 一组可挂载的业务服务实现，集中在 `service/`
- 示例工程，包括基础示例与 `0x2A + ULOG` 周期上报示例

> 当前仓库实际内容以 RT-Thread 服务端组件为主，README 已按现有代码结构重新整理。

---

## 1. 仓库定位

适用场景包括：

- MCU / ECU 诊断接入
- 产线测试、售后维护、远程运维
- DID 参数读写
- `0x2F` I/O 控制
- `0x2A` 周期数据或日志流输出
- 基于 `0x31` 的远程控制台
- 基于 `0x34 ~ 0x38` 的文件读写流程

这不是一个“只给协议核心”的仓库，而是一个 **RT-Thread 侧可落地的 UDS 集成框架**。

---

## 2. 代码结构

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

## 3. 已覆盖服务

| SID | 服务 | 实现文件 | 说明 |
|---|---|---|---|
| 0x10 | Diagnostic Session Control | `service_0x10_session.c` | default / extended / programming |
| 0x11 | ECU Reset | `service_0x11_reset.c` | 正响应后执行延迟复位 |
| 0x22 | ReadDataByIdentifier | `service_0x22_0x2E_param.c` | 参数读取适配 |
| 0x27 | Security Access | `service_0x27_security.c` | Seed / Key 解锁 |
| 0x28 | Communication Control | `service_0x28_comm.c` | 应用收发控制 |
| 0x2A | ReadDataByPeriodicIdentifier | `service_0x2A_periodic.c` | provider 注册与调度 |
| 0x2E | WriteDataByIdentifier | `service_0x22_0x2E_param.c` | 参数写入适配 |
| 0x2F | InputOutputControlByIdentifier | `service_0x2F_io.c` | 控制权接管 / 归还 |
| 0x31 | RoutineControl | `service_0x31_console.c` | 远程控制台例程 |
| 0x34/0x36/0x37/0x38 | 文件传输相关服务 | `service_0x36_0x37_0x38_file.c` | 文件读写与 CRC 收尾 |
| 0x3E | TesterPresent | core | 会话保活 |

---

## 4. Client / Tester 实现

本仓库主要提供 **UDS Server（ECU 侧）实现**。

如果你需要一个配套的诊断客户端（Tester），可以参考以下仓库中的 Linux 实现：

👉 https://github.com/wdfk-prog/iso14229/tree/rtt/examples/rtt_server/client_demo

该示例基于 Linux + SocketCAN / ISO-TP，可用于：

- 与本仓库 server 进行联调
- 验证各类 UDS 服务（0x10 / 0x22 / 0x31 / 文件传输等）
- 作为产线或调试工具的基础实现

> 建议：在 bring-up 阶段优先使用该 client_demo 进行协议联调。

## 5. 快速接入

1. 在 `menuconfig` 中开启软件包和所需服务。
2. 确认 BSP 的 CAN 驱动已可正常收发。
3. 通过 `rtt_uds_create()` 创建运行环境。
4. 通过 `rtt_uds_*_service_mount()` 挂载服务。
5. 驱动收到 CAN 帧后调用 `rtt_uds_feed_can_frame()` 喂入协议栈。

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

详细接入说明见：[`docs/zh/quick-start.md`](docs/zh/quick-start.md)

---

## 6. 文档索引（中文）

- [`docs/zh/architecture.md`](docs/zh/architecture.md)
- [`docs/zh/quick-start.md`](docs/zh/quick-start.md)
- [`docs/zh/service-guide.md`](docs/zh/service-guide.md)
- [`docs/zh/did-design.md`](docs/zh/did-design.md)
- [`docs/zh/file-transfer.md`](docs/zh/file-transfer.md)
- [`docs/zh/console-service.md`](docs/zh/console-service.md)
- [`docs/zh/0x2a-periodic.md`](docs/zh/0x2a-periodic.md)
- [`docs/zh/porting-notes.md`](docs/zh/porting-notes.md)

---

## 7. English docs

See [`README.md`](README.md) for the English entry, or go directly to [`docs/en/`](docs/en/).

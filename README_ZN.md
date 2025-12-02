[English](README.md)

## 1. 项目概述 (Project Overview)

基于RT-Thread操作系统的ISO 14229 (Unified Diagnostic Services, UDS) 诊断工具实现，包含服务端(server)和客户端(client)两部分。服务端运行在嵌入式设备上，通过CAN总线与客户端通信，实现了多种UDS诊断服务，方便对嵌入式设备进行远程诊断和控制。

本项目旨在为RT-Thread用户提供一套完整的UDS诊断解决方案，不仅可以直接使用，也可以作为学习UDS协议和开发自定义诊断功能的参考。

### 主要特点
- 基于RT-Thread操作系统，充分利用其多任务和设备驱动框架
- 实现了常用的UDS诊断服务，满足大部分诊断需求
- 提供跨平台的Linux客户端，方便与服务端进行交互
- 模块化设计，易于扩展新的UDS服务
- 包含丰富的示例代码，帮助用户快速上手

### 应用场景
- 汽车电子ECU诊断
- 工业控制器远程维护
- 嵌入式设备固件升级
- 设备状态监控和故障排查

---

## 2. 功能特性 (Features)

### 服务端特性 (Server Features)
- 基于RT-Thread的多任务架构，保证系统稳定性和实时性
- 完整的ISO 14229 UDS协议栈实现
- 支持多种UDS诊断服务，包括会话控制、安全访问、参数读写等
- 灵活的服务注册机制，便于扩展新功能
- 与RT-Thread CAN设备驱动无缝集成
- 支持CAN总线通信控制，可动态启用/禁用消息收发

### 客户端特性 (Client Features)
- 基于Linux平台，使用SocketCAN进行CAN通信
- 提供命令行交互界面，支持命令历史和自动补全
- 实现了与服务端配套的UDS服务客户端功能
- 支持文件上传下载功能
- 支持远程控制台命令执行
- 具备完善的错误处理和日志显示功能

### 通信特性 (Communication Features)
- 基于ISO TP (ISO 15765-2)的CAN总线数据传输协议
- 支持大数据块传输，适用于文件传输等场景
- 具备流量控制机制，保证通信稳定性
- 支持功能寻址和物理寻址两种通信方式

---

## 3. 支持的UDS服务 (Supported UDS Services)

本项目实现了以下ISO 14229标准定义的UDS服务：

| Service ID | Name                          | Description                            |
|------------|-------------------------------|----------------------------------------|
| 0x10       | Diagnostic Session Control    | 控制诊断会话模式（默认/扩展会话/编程会话） |
| 0x11       | ECU Reset                     | 执行ECU复位操作                         |
| 0x22       | Read Data By Identifier       | 读取数据标识符对应的参数值               |
| 0x27       | Security Access               | 安全访问服务，用于解锁高级诊断功能       |
| 0x28       | Communication Control         | 控制通信行为（启用/禁用消息收发）        |
| 0x2E       | Write Data By Identifier      | 写入数据标识符对应的参数值               |
| 0x2F       | Input/Output Control          | 控制输入输出信号的行为                  |
| 0x31       | Routine Control               | 控制例程的启动、停止和结果查询          |
| 0x34       | Request Download              | 请求下载数据到ECU                       |
| 0x36       | Transfer Data                 | 传输数据块                              |
| 0x37       | Request Transfer Exit         | 结束数据传输                            |
| 0x38       | Request File Transfer         | 请求文件传输                            |
| 0x3E       | Tester Present                | 测试仪在线保持                          |

这些服务涵盖了UDS协议的核心功能，能够满足大多数诊断应用场景的需求。

---

## 4. 系统架构 (System Architecture)

### 整体架构图
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

### 服务端架构
服务端基于RT-Thread操作系统，采用分层架构设计：
1. **硬件抽象层**：RT-Thread CAN设备驱动
2. **传输层**：ISO TP协议实现
3. **协议层**：ISO 14229核心协议栈 (iso14229_rtt.c)
4. **服务层**：各种UDS服务的具体实现 (service/)
5. **应用层**：用户自定义的应用逻辑 (rtt_uds_example.c)

### 客户端架构
客户端基于Linux平台，同样采用分层架构设计：
1. **硬件抽象层**：SocketCAN接口
2. **传输层**：ISO TP协议实现
3. **协议层**：ISO 14229核心协议栈 (iso14229.c)
4. **服务层**：各种UDS服务的客户端实现 (services/)
5. **应用层**：命令行交互界面和用户命令处理 (core/, main.c)

---

## 5. 代码结构 (Code Structure)

### 服务端代码结构
```
server_demo/
├── examples/                    # 示例应用
│   └── rtt_uds_example.c       # RT-Thread UDS服务端示例
├── service/                     # UDS服务实现
│   ├── rtt_uds_service.h       # 服务定义宏和API
│   ├── service_0x10_session.c  # 0x10会话控制服务
│   ├── service_0x11_reset.c    # 0x11 ECU复位服务
│   ├── service_0x22_0x2E_param.c # 0x22/0x2E参数读写服务
│   ├── service_0x27_security.c # 0x27安全访问服务
│   ├── service_0x28_comm.c     # 0x28通信控制服务
│   ├── service_0x2F_io.c       # 0x2F输入输出控制服务
│   ├── service_0x31_console.c  # 0x31远程控制台服务
│   └── service_0x36_0x37_0x38_file.c # 0x36/0x37/0x38文件传输服务
├── iso14229_rtt.c              # UDS协议核心实现(RT-Thread版)
├── iso14229_rtt.h              # UDS协议核心头文件
└── rtt_uds_config.h            # RT-Thread平台配置文件
```

### 客户端代码结构
```
client_demo/
├── core/                        # 核心模块
│   ├── client.h                # 客户端公共头文件
│   ├── client_config.c/h       # 客户端配置管理
│   ├── client_shell.c/h        # 命令行交互界面
│   ├── cmd_registry.c/h        # 命令注册管理
│   ├── response_registry.c/h   # 响应注册管理
│   └── uds_context.c/h         # UDS上下文管理
├── services/                    # UDS服务客户端实现
│   ├── client_0x10_session.c   # 0x10会话控制客户端
│   ├── client_0x11_reset.c     # 0x11 ECU复位客户端
│   ├── client_0x22_0x2E_param.c # 0x22/0x2E参数读写客户端
│   ├── client_0x27_security.c  # 0x27安全访问客户端
│   ├── client_0x28_comm.c      # 0x28通信控制客户端
│   ├── client_0x2F_io.c        # 0x2F输入输出控制客户端
│   ├── client_0x31_console.c   # 0x31远程控制台客户端
│   └── client_0x36_0x37_0x38_file.c # 0x36/0x37/0x38文件传输客户端
├── utils/                       # 工具模块
│   ├── linenoise.c/h           # 命令行编辑库
│   └── utils.c/h               # 通用工具函数
├── main.c                      # 主程序入口
└── Makefile                    # 构建脚本
```

---

## 6. 硬件要求 (Hardware Requirements)

### 服务端 (Server Side)
- 运行RT-Thread操作系统的嵌入式开发板
- 至少具备一个CAN控制器的硬件接口
- 连接CAN收发器（如TJA1050、SN65HVD230等）
- 用于LED控制示例的GPIO引脚（可选）

### 客户端 (Client Side)
- 运行Linux操作系统的主机（物理机或虚拟机）
- 支持SocketCAN的CAN接口适配器
  - USB转CAN适配器（如USBCAN-I、USBCAN-II）
  - PCIe CAN接口卡
  - 具备CAN控制器的开发板（如树莓派配合MCP2515）
- CAN总线连接线缆和终端电阻

---

## 7. 快速开始 (Getting Started)

### 服务端设置 (Server Setup)

1. **配置**
    - 在 RT-Thread 的 `menuconfig` 中启用该软件包：

    ```sh
    RT-Thread online packages  --->
        peripherals packages  --->
            [*] Enable iso14229 (UDS) library  --->
                (32) Event Dispatch Table Size
                [*] Enable UDS server example application
    ```

2. **集成库文件**
   - 将`server_demo`目录下的源文件添加到您的RT-Thread项目中
   - 确保项目配置中启用了CAN设备驱动支持

3. **编译和烧录**
   - 使用RT-Thread支持的工具链编译项目
   - 将生成的固件烧录到目标硬件

4. **运行示例**
   - 通过串口终端连接到设备控制台
   - 启动UDS服务端示例：
     ```
     msh />uds_example start can1
     ```

### 客户端设置 (Client Setup)

1. **配置SocketCAN**
   - 在Linux主机上启用并配置CAN接口：
     ```bash
     # 加载CAN模块（如果需要）
     sudo modprobe can
     sudo modprobe can_raw
     sudo modprobe vcan  # 用于虚拟CAN测试
     
     # 配置物理CAN接口（以1Mbps为例）
     sudo ip link set can0 up type can bitrate 1000000
     
     # 或创建虚拟CAN接口用于测试
     sudo ip link add dev vcan0 type vcan
     sudo ip link set up vcan0
     ```

2. **编译客户端**
   - 进入`client_demo`目录
   - 修改`Makefile`中的交叉编译工具链路径（如果需要）
   - 执行编译命令：
     ```bash
     make                    # 交叉编译（默认）
     make NATIVE=1          # 本地编译用于测试
     ```

3. **运行客户端**
   - 执行生成的客户端程序：
     ```bash
     ./client -i can0 -s 7E8 -t 7E0  # 使用can0接口
     ./client -i vcan0               # 使用虚拟CAN接口
     ```

---

## 8. 使用方法 (Usage)

### 服务端使用 (Server Usage)

通过RT-Thread的MSH命令行界面控制UDS服务端：

```bash
# 启动UDS服务端
msh />uds_example start can1

# 停止UDS服务端
msh />uds_example stop can1

# 查看已注册的服务
msh />uds_list
```

服务端启动后将在指定的CAN接口上监听诊断请求，并根据配置提供相应的服务功能。

### 客户端使用 (Client Usage)

客户端提供交互式命令行界面，支持多种诊断命令：

```bash
# 启动客户端（默认使用can1接口）
$ ./client

# 启动客户端并指定接口和地址
$ ./client -i vcan0 -s 7E8 -t 7E0

# 进入交互界面后的常用命令：
UDS> help              # 显示帮助信息
UDS> session 03        # 切换到扩展会话
UDS> security 01       # 执行安全访问（级别01）
UDS> wdbi 0001 01      # 写入DID 0001值为01
UDS> io 0100 03 01 00 00  # 控制IO（强制红色LED）
UDS> sy local_file.bin # 上传文件到服务端
UDS> ry remote_file.bin # 从服务端下载文件
UDS> rexec ps          # 在服务端执行命令
UDS> cd /              # 切换服务端目录
UDS> lls               # 列出本地文件
UDS> exit              # 退出客户端
```

---

## 9. 服务详解 (Service Details)

### 0x10 会话控制 (Diagnostic Session Control)
控制ECU的诊断会话模式，不同会话模式具有不同的安全级别和功能访问权限：
- 默认会话 (0x01)：基本诊断功能
- 编程会话 (0x02)：用于程序刷写
- 扩展会话 (0x03)：访问更多诊断功能

### 0x27 安全访问 (Security Access)
通过种子-密钥机制保护敏感诊断功能：
1. 客户端请求特定安全级别的种子
2. 服务端返回随机种子值
3. 客户端根据算法计算密钥
4. 客户端发送密钥进行验证
5. 验证通过后解锁对应的安全级别

### 0x2F 输入输出控制 (Input/Output Control)
控制ECU的数字量输入输出信号：
- 强制控制 (Short Term Adjustment)：临时改变IO状态
- 返回控制 (Return Control)：恢复IO控制权给应用程序
- 冻结状态 (Freeze Current State)：锁定当前IO状态
- 复位默认 (Reset To Default)：恢复IO到默认状态

### 0x31 远程控制台 (Remote Console)
通过UDS服务在服务端执行命令行指令，实现远程控制功能：
- 执行任意MSH命令
- 获取命令执行结果
- 支持目录切换和文件浏览

### 0x36/0x37/0x38 文件传输 (File Transfer)
完整的文件传输功能，支持上传和下载：
1. 0x38 Request File Transfer：协商文件传输参数
2. 0x36 Transfer Data：传输文件数据块
3. 0x37 Request Transfer Exit：结束传输并校验完整性

---

## 10. 示例和日志 (Examples and Logs)

### 典型交互日志

```sh
# 客户端启动
$ ./client -i vcan0
[Config] IF: vcan0 | SA: 0x7E8 | TA: 0x7E0 | FUNC: 0x7DF

[Shell] Interactive Mode Started. Type 'help' or 'exit'.

UDS> 

# 切换到扩展会话
UDS> session 03
[I] (940) Session: Requesting Session Control: 0x03
[+] Switching Session Done.   
[I] (950) Session: Session Switched Successfully (0x03)

# 执行安全访问
UDS> security 01
[I] (1020) Sec: Starting Security Access (Level 0x01)...
[+] Requesting Seed Done.   
[I] (1030) Sec: Seed: 0x12345678 -> Key: 0xB791F3D5
[+] Verifying Key Done.   
[I] (1040) Sec: Security Access Granted!

# 控制LED
UDS> io 0100 03 01 00 00
[I] (1120) IO Ctrl: Sending IO Control: DID=0x0100 Param=0x03
[+] Requesting Done.   

# 上传文件
UDS> sy test.txt
[*] Uploading 'test.txt' (1024 bytes)...
[+] Requesting Done.   
[=>] 1024/1024 bytes
[+] Finishing Done.   
[+] Upload Complete.

# 执行远程命令
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

## 11. 开发指南 (Development Guide)

### 添加新的UDS服务

1. 在服务端：
   - 在`service/`目录创建新的服务实现文件
   - 实现服务处理函数
   - 注册服务节点到UDS环境

2. 在客户端：
   - 在`services/`目录创建客户端实现文件
   - 实现命令处理函数
   - 注册命令到命令管理系统

### 配置说明

客户端Makefile中的关键配置：
```makefile
# ISO-TP流控参数优化，提高传输效率
CFLAGS += -DISOTP_FC_BS=0      # 块大小设为0（无流控）
CFLAGS += -DISOTP_FC_STMIN=0   # 最小间隔时间为0
```

---
## 12. 相关链接
- [iso14229 code](https://github.com/driftregion/iso14229/)
- [iso14229 client demo](https://github.com/wdfk-prog/iso14229/tree/rtt/examples/rtt_server/client_demo)
- [iso14229 rtt software](https://github.com/wdfk-prog/can_uds)


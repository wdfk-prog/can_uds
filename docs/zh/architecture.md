# 架构说明

## 1. 分层

仓库可以按四层理解：

1. **协议核心层**：`iso14229.c` / `iso14229.h`
2. **RT-Thread 适配层**：`iso14229_rtt.c` / `iso14229_rtt.h`
3. **业务服务层**：`service/*.c` + `service/rtt_uds_service.h`
4. **应用示例层**：`examples/*.c`

## 2. 运行时对象

核心运行时对象是 `rtt_uds_env_t`，它负责：

- 持有 UDS server 上下文
- 维护事件分发
- 接收 CAN 帧并转换为协议输入
- 将服务节点挂接到事件表

## 3. 业务服务的接入方式

各业务模块并不直接改动核心状态机，而是通过：

- `uds_service_node_t`
- `RTT_UDS_SERVICE_NODE_INIT(...)`
- `rtt_uds_service_register(...)`

把自身注册为某个 `UDSEvent_t` 的处理节点。

## 4. 典型数据流

```text
CAN RX IRQ / driver
    -> rt_can_msg
    -> rtt_uds_feed_can_frame()
    -> iso14229 core decode
    -> event dispatch
    -> service handler
    -> positive / negative response
    -> CAN TX
```

## 5. 设计取舍

### 5.1 核心保持协议职责

协议核心负责：

- SID 解析
- session / security / timeout 等通用状态
- request / response 封装
- 事件回调入口

### 5.2 业务层负责项目策略

业务层负责：

- DID 与参数系统映射
- I/O 控制语义
- 文件系统访问
- 远程控制台绑定
- 周期上报策略

这种拆分让 `service/` 更像“官方参考适配”，而不是把所有项目逻辑写死到协议核心里。

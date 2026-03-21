# 服务总览

## 0x10 Session

用于会话切换。通常作为其他敏感服务的前置条件。

## 0x11 ECU Reset

采用“两阶段”处理：

- 第一阶段校验请求并返回正响应
- 第二阶段在响应发出后执行物理复位

## 0x22 / 0x2E DID 参数服务

这是项目适配层，不是通用参数数据库。你需要替换成自己的参数/NVM 后端。

## 0x27 Security Access

当前实现提供示例级 Seed/Key 机制，适合联调，不适合作为量产安全方案直接使用。

## 0x28 Communication Control

用于关闭或恢复应用收发逻辑。可用于下载、刷写或静默测试等场景。

## 0x2A 周期数据

以 provider 方式注册 PDID，再由 0x2A 服务管理订阅与调度。

## 0x2F IO Control

提供 DID 到具体 I/O 处理器的映射，并支持会话超时后自动 `ReturnControlToECU`。

## 0x31 Remote Console

把 shell 命令执行能力包装为一个 Routine，返回命令输出。

## 0x34 ~ 0x38 File Transfer

面向文件读写场景，支持：

- request file transfer
- transfer data
- request transfer exit
- CRC32 收尾校验

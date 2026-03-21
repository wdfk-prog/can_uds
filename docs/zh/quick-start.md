# 快速接入

## 1. 使能软件包

在 `menuconfig` 中使能：

- `PKG_USING_CAN_UDS`
- 需要的服务开关
- 示例开关（可选）

## 2. 初始化顺序建议

1. CAN 驱动 ready
2. 创建 `rtt_uds_env_t`
3. 注册各业务服务
4. 注册硬件节点 / provider / DID 映射
5. 启动 UDS 工作线程或示例

## 3. 创建环境

```c
rtt_uds_config_t cfg = {0};
rtt_uds_env_t *env = rtt_uds_create(&cfg);
```

## 4. 挂载服务

```c
rtt_uds_sec_service_mount(env, &security_service);
rtt_uds_io_service_mount(env, &io_service);
rtt_uds_file_service_mount(env, &file_service);
rtt_uds_0x2a_service_mount(env, &periodic_service);
```

## 5. 喂入 CAN 帧

驱动收到帧后，向环境输入：

```c
rtt_uds_feed_can_frame(env, &msg);
```

## 6. 初次调试建议

- 先只开 `0x10 + 0x11 + 0x3E`
- 再逐步叠加 `0x27 / 0x22 / 0x2E / 0x2F`
- 文件服务与 console 服务最后接入
- 若开 `0x2A`，先只注册单个 provider 验证周期调度

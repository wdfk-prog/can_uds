# 0x2A 周期数据服务

## 1. 实现定位

`service/service_0x2A_periodic.c` 是一个 **RT-Thread 业务适配层**，不是把 0x2A 逻辑全部固化到 UDS 核心中。

其职责是：

- 注册 provider
- 管理 PDID 订阅状态
- 根据 transmission mode 调度发送
- 按 round-robin 输出周期数据

## 2. provider 模型

每个 PDID 对应一个 `uds_0x2a_provider_t`，通常包括：

- `data_id`
- `check()`：访问检查（可选）
- `read()`：生成 payload
- `context`

## 3. 关键流程

### 3.1 注册

```c
uds_0x2a_register_provider(&svc, &provider);
```

### 3.2 订阅

当 tester 发来 0x2A 请求时，服务会：

- 定位 PDID
- 校验 transmission mode
- 执行可选 `check()`
- 置位 `subscribed`

### 3.3 发送

发送阶段不会一次把所有 provider 都塞满，而是按 `rr_cursor` 做轮询，逐次返回一个订阅项的当前 payload。

## 4. 这样设计的好处

- 避免核心层感知具体业务 DID
- 不同 PDID 可以绑定不同数据源
- 方便把日志流、传感器值、状态字统一抽象成 provider
- 易于裁剪和测试

## 5. 示例：ULOG 周期输出

仓库中的 `examples/rtt_uds_0x2a_ulog_example.c` 展示了一个很有代表性的用法：

- 用 ringbuffer 缓存日志流
- 用 provider 的 `read()` 从 ringbuffer 中取一段数据
- 以 0x2A 周期数据方式把日志内容送给 tester

## 6. 对接建议

- 把高速、低速 PDID 分组
- 对 payload 长度做严格边界控制
- `check()` 中加入会话/安全级别限制
- 对日志类 provider 做节流和丢包策略设计

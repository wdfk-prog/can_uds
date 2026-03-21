# Architecture Overview

## 1. Layering

The repository can be understood as four layers:

1. **Protocol core layer**: `iso14229.c` / `iso14229.h`
2. **RT-Thread adaptation layer**: `iso14229_rtt.c` / `iso14229_rtt.h`
3. **Business service layer**: `service/*.c` + `service/rtt_uds_service.h`
4. **Application example layer**: `examples/*.c`

## 2. Runtime object

The main runtime object is `rtt_uds_env_t`. It is responsible for:

- holding the UDS server context
- maintaining event dispatching
- receiving CAN frames and converting them into protocol input
- attaching service nodes to the event table

## 3. How business services are integrated

Business modules do not modify the protocol core state machine directly. Instead, they are connected through:

- `uds_service_node_t`
- `RTT_UDS_SERVICE_NODE_INIT(...)`
- `rtt_uds_service_register(...)`

so that each module can register itself as a handler for one `UDSEvent_t`.

## 4. Typical data flow

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

## 5. Design split

### 5.1 The core stays protocol-focused

The protocol core is responsible for:

- SID parsing
- common session / security / timeout state
- request / response packaging
- event callback entry points

### 5.2 The business layer owns project policy

The business layer is responsible for:

- DID to parameter-system mapping
- I/O control semantics
- file-system access
- remote console binding
- periodic-reporting policy

This split makes `service/` behave more like an official reference adaptation layer, instead of hard-coding project logic into the protocol core.

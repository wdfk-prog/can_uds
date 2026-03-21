# Porting and Trimming Notes

## 1. Separate what is “core” from what is “project-specific”

### More generic parts

- `iso14229.c`
- `iso14229.h`
- `iso14229_rtt.c`
- `iso14229_rtt.h`
- generic service framework and event dispatch

### Strongly project-specific parts

- `service_0x22_0x2E_param.c`
- `service_0x31_console.c`
- `service_0x36_0x37_0x38_file.c`
- LED / ULOG logic in the examples

## 2. Priority checks when porting to a new BSP

1. CAN driver and RX/TX path
2. whether RT-Thread console / FinSH can be switched
3. whether DFS / POSIX file APIs are available
4. thread priority and memory sizing
5. whether ULOG is enabled

## 3. Common trimming strategies

### Minimal diagnostic set

- `0x10`
- `0x11`
- `0x3E`
- optional `0x22`

### Production-line parameter set

- `0x10`
- `0x22 / 0x2E`
- `0x27`
- `0x3E`

### Remote-maintenance set

- `0x10`
- `0x27`
- `0x2A`
- `0x31`
- `0x34~0x38`
- `0x3E`

## 4. What you will most likely need to change yourself

- security algorithm
- DID table design
- file directory and permission control
- shell command whitelist
- IO control binding
- unified package macro naming

## 5. Recommended pre-release checklist

- whether session-timeout behavior matches expectations
- whether auto-return after `0x2F` takeover is reliable
- whether failed file transfer leaves dirty files behind
- whether console service has dangerous command-injection risk
- whether heavy 0x2A traffic affects the main business thread

# Remote Console Service

## 1. Design target

Expose a standard UDS `0x31 RoutineControl` entry for remote shell command execution, so it can be used for:

- on-site debugging
- log collection
- status inspection
- simple maintenance operations

## 2. Implementation approach

The current implementation is in `service/service_0x31_console.c`. The core idea is:

1. create a virtual character device
2. temporarily switch RT-Thread console / FinSH output to that device
3. execute the MSH command
4. capture the output text
5. return it to the diagnostic client through `statusRecord`

## 3. Request constraints

The default logic includes the following protections:

- extended session can be required
- security unlock can be required
- only `UDS_LEV_RCTP_STR` is accepted
- only the configured RID is accepted (default `0xF000`)

## 4. Response content

The response typically looks like:

```text
> ps
thread   pri status ...
...
```

That is, the command string and the shell output are returned together.

## 5. Risks and notes

### 5.1 Security risk

This is a high-privilege capability. For production products, it is recommended to enforce at least:

- only extended / programming session can use it
- `0x27` unlock is mandatory
- command whitelist
- output-length limit
- audit logging

### 5.2 Output truncation

The current buffer size is controlled by `UDS_CONSOLE_BUF_SIZE`. When the output exceeds the buffer, a `[TRUNCATED]` marker is appended.

### 5.3 Coexistence with physical serial console

If `UDS_CONSOLE_PASSTHROUGH` is enabled, output continues to the original console as well. Otherwise it is visible only in the capture buffer.

## 6. Future extension ideas

- command whitelist table
- multi-frame output continuation
- paged retrieval
- separated stdout / stderr
- asynchronous routine mode for long-running commands

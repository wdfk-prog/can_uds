# 0x2A Periodic Data Service

## 1. Implementation position

`service/service_0x2A_periodic.c` is an **RT-Thread business adaptation layer**. It does not hard-code the whole 0x2A logic into the UDS core.

Its responsibilities are:

- register providers
- manage PDID subscription state
- schedule transmission according to transmission mode
- output periodic data in round-robin order

## 2. Provider model

Each PDID corresponds to one `uds_0x2a_provider_t`, which usually contains:

- `data_id`
- `check()`: optional access check
- `read()`: generate payload
- `context`

## 3. Key flow

### 3.1 Registration

```c
uds_0x2a_register_provider(&svc, &provider);
```

### 3.2 Subscription

When the tester sends a 0x2A request, the service will:

- locate the PDID
- validate the transmission mode
- run the optional `check()`
- set `subscribed`

### 3.3 Transmission

During transmission, the service does not pack all providers into one response. Instead, it uses `rr_cursor` to iterate in round-robin order and returns the current payload of one subscribed provider at a time.

## 4. Why this design helps

- the core layer does not need to understand business DIDs
- different PDIDs can bind to different data sources
- log streams, sensor values, and status words can all be abstracted as providers
- it is easy to trim and test

## 5. Example: periodic ULOG output

The repository example `examples/rtt_uds_0x2a_ulog_example.c` shows a representative usage:

- use a ringbuffer to cache log data
- use the provider `read()` callback to fetch one chunk from the ringbuffer
- stream the log content to the tester through 0x2A periodic data

## 6. Integration advice

- group high-rate and low-rate PDIDs separately
- enforce strict payload-length limits
- add session / security-level checks inside `check()`
- design throttling and drop policy for log-style providers

# Quick Start

## 1. Enable the package

Enable the following items in `menuconfig`:

- `PKG_USING_CAN_UDS`
- the service switches you actually need
- example switches (optional)

## 2. Recommended initialization order

1. Make sure the CAN driver is ready
2. Create `rtt_uds_env_t`
3. Register the business services you need
4. Register hardware nodes / providers / DID mappings
5. Start the UDS worker thread or the example entry

## 3. Create the environment

```c
rtt_uds_config_t cfg = {0};
rtt_uds_env_t *env = rtt_uds_create(&cfg);
```

## 4. Mount services

```c
rtt_uds_sec_service_mount(env, &security_service);
rtt_uds_io_service_mount(env, &io_service);
rtt_uds_file_service_mount(env, &file_service);
rtt_uds_0x2a_service_mount(env, &periodic_service);
```

## 5. Feed CAN frames

After the driver receives a frame, pass it into the environment:

```c
rtt_uds_feed_can_frame(env, &msg);
```

## 6. Recommended first-debug path

- start with only `0x10 + 0x11 + 0x3E`
- then add `0x27 / 0x22 / 0x2E / 0x2F` step by step
- integrate file transfer and console service last
- if `0x2A` is enabled, start with a single provider to verify periodic scheduling

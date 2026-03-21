# File Transfer Design

## 1. Scope

The current implementation is centered in `service_0x36_0x37_0x38_file.c`, but at the business level it corresponds to the following flow:

- `0x38 RequestFileTransfer`
- `0x36 TransferData`
- `0x37 RequestTransferExit`
- plus the negotiation fields used by download / upload

## 2. Runtime context

The file-service context keeps the following state:

- `fd`
- `current_path`
- `mode`
- `current_pos`
- `total_size`
- `current_crc`

## 3. Typical download flow (client -> ECU)

```text
RequestFileTransfer(mode = add/replace)
    -> server open(path, O_WRONLY | O_CREAT | O_TRUNC)
    -> negotiate max block length
TransferData(block #1..N)
    -> server write()
    -> update crc32 / current_pos
RequestTransferExit(crc32)
    -> compare client crc and server crc
    -> success: close file
    -> failure: close + unlink partial file
```

## 4. Typical upload flow (ECU -> client)

```text
RequestFileTransfer(mode = read)
    -> server open(path, O_RDONLY)
    -> query file size
TransferData(loop)
    -> server read()
    -> copyResponse()
RequestTransferExit()
    -> server returns crc32 in response record
```

## 5. Design characteristics

### 5.1 Block-length negotiation

The maximum block length is constrained by two limits together:

- protocol limit: `UDS_ISOTP_MTU - 2`
- implementation limit: `UDS_FILE_CHUNK_SIZE`

The smaller value is used.

### 5.2 CRC validation

For file-write scenarios, CRC32 is checked during `TransferExit`. If the check fails, the current implementation deletes the partially written target file to avoid accidental use of incomplete data.

### 5.3 Session-timeout cleanup

The session-timeout event closes any still-open file handle, reducing the risk of resource leakage.

## 6. Recommended hardening

- restrict flashing directories by whitelist
- add path normalization and path traversal checks
- distinguish temporary files from final files
- use atomic rename after success
- bind the service to `0x27` and programming session rules
- add resume / recovery strategy for large-file workflows when needed

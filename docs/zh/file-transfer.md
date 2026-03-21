# 文件传输设计

## 1. 覆盖范围

当前实现集中在 `service_0x36_0x37_0x38_file.c`，但业务上对应以下流程：

- `0x38 RequestFileTransfer`
- `0x36 TransferData`
- `0x37 RequestTransferExit`
- 以及下载/上传协商相关字段

## 2. 运行时上下文

文件服务上下文维护以下状态：

- `fd`
- `current_path`
- `mode`
- `current_pos`
- `total_size`
- `current_crc`

## 3. 典型下载流程（客户端 -> ECU）

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

## 4. 典型上传流程（ECU -> 客户端）

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

## 5. 设计特点

### 5.1 块长协商

最大块长由两部分共同约束：

- 协议限制：`UDS_ISOTP_MTU - 2`
- 实现限制：`UDS_FILE_CHUNK_SIZE`

最终取两者最小值。

### 5.2 CRC 校验

写文件场景在 `TransferExit` 阶段校验 CRC32；若失败，当前实现会删除已写入的目标文件，避免残缺文件被误用。

### 5.3 会话超时清理

会话超时事件会关闭仍处于打开状态的文件句柄，减少资源泄漏风险。

## 6. 实现建议

- 为刷写目录做白名单限制
- 增加路径规范化与越界校验
- 区分临时文件与正式文件
- 成功后原子 rename
- 与 0x27/编程会话绑定
- 对大文件场景增加断点续传或外层恢复机制

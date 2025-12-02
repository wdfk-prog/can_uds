/**
 * @file service_0x36_0x37_0x38_file.c
 * @brief UDS File Transfer Service Implementation (Context-Based).
 */

#include "rtt_uds_service.h"
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef RT_USING_DFS
#include <dfs_file.h>
#endif

#define DBG_TAG "uds.file"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef UDS_ENABLE_FILE_SVC

#ifndef UDS_FILE_CHUNK_SIZE
#define UDS_FILE_CHUNK_SIZE  1024
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* ==========================================================================
 * Helper Functions
 * ========================================================================== */

static uint32_t crc32_calc(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc; 
    while (len--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

/* ==========================================================================
 * Service Handlers
 * ========================================================================== */

static UDS_HANDLER(handle_file_request)
{
    /* [Key Change] Get context from pointer */
    uds_file_service_t *ctx = (uds_file_service_t *)context;
    if (!ctx) return UDS_NRC_ConditionsNotCorrect;

    UDSRequestFileTransferArgs_t *args = (UDSRequestFileTransferArgs_t *)data;
    char path[UDS_FILE_MAX_PATH_LEN];
    struct stat fstat_res;
    
    /* 1. Cleanup previous session */
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
        ctx->mode = FILE_MODE_IDLE;
    }
    
    /* 2. Path Handling */
    if (args->filePathLen >= sizeof(path)) return UDS_NRC_RequestOutOfRange;
    rt_memcpy(path, args->filePath, args->filePathLen);
    path[args->filePathLen] = '\0';
    
    rt_strncpy(ctx->current_path, path, sizeof(ctx->current_path) - 1);

    /* 3. Negotiate Block Length */
    uint16_t proto_limit = UDS_ISOTP_MTU - 2;
    uint16_t mem_limit   = UDS_FILE_CHUNK_SIZE;
    args->maxNumberOfBlockLength = MIN(proto_limit, mem_limit);

    ctx->current_crc = 0; 

    /* 4. Handle Operation */
    switch (args->modeOfOperation) {
        case UDS_MOOP_ADDFILE:
        case UDS_MOOP_REPLFILE:
            ctx->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0);
            if (ctx->fd < 0) return UDS_NRC_ConditionsNotCorrect;
            
            ctx->total_size = (uint32_t)args->fileSizeUnCompressed;
            ctx->current_pos = 0;
            ctx->mode = FILE_MODE_WRITING;
            return UDS_PositiveResponse;

        case UDS_MOOP_RDFILE:
            ctx->fd = open(path, O_RDONLY, 0);
            if (ctx->fd < 0) return UDS_NRC_RequestOutOfRange;
            
            if (stat(path, &fstat_res) < 0) {
                close(ctx->fd);
                ctx->fd = -1;
                return UDS_NRC_ConditionsNotCorrect;
            }
            
            ctx->total_size = (uint32_t)fstat_res.st_size;
            ctx->current_pos = 0;
            ctx->mode = FILE_MODE_READING;
            
            args->fileSizeUnCompressed = fstat_res.st_size;
            args->fileSizeCompressed = fstat_res.st_size; 
            return UDS_PositiveResponse;
            
        default:
            return UDS_NRC_SubFunctionNotSupported;
    }
}

static UDS_HANDLER(handle_transfer_data)
{
    uds_file_service_t *ctx = (uds_file_service_t *)context;
    if (!ctx || ctx->fd < 0) return UDS_NRC_ConditionsNotCorrect;

    UDSTransferDataArgs_t *args = (UDSTransferDataArgs_t *)data;
    int rw_len;
    UDSErr_t result = UDS_PositiveResponse;

    if (ctx->mode == FILE_MODE_WRITING) {
        rw_len = write(ctx->fd, args->data, args->len);
        if (rw_len != args->len) return UDS_NRC_GeneralProgrammingFailure;
        
        ctx->current_pos += rw_len;
        ctx->current_crc = crc32_calc(ctx->current_crc, args->data, args->len);
        return UDS_PositiveResponse;
    }
    else if (ctx->mode == FILE_MODE_READING) {
        /* [Alloc] Dynamic allocation as requested previously to allow large chunks */
        uint8_t *read_buf = rt_malloc(UDS_FILE_CHUNK_SIZE);
        if (!read_buf) return UDS_NRC_ConditionsNotCorrect;
        
        uint16_t bytes_to_read = MIN(UDS_FILE_CHUNK_SIZE, args->maxRespLen);
        rw_len = read(ctx->fd, read_buf, bytes_to_read);
        
        if (rw_len < 0) {
            result = UDS_NRC_GeneralProgrammingFailure;
        } else if (rw_len > 0) {
            ctx->current_pos += rw_len;
            ctx->current_crc = crc32_calc(ctx->current_crc, read_buf, rw_len);
            result = args->copyResponse(srv, read_buf, (uint16_t)rw_len);
        } else {
            result = args->copyResponse(srv, read_buf, 0);
        }
        
        rt_free(read_buf);
        return result;
    }

    return UDS_NRC_ConditionsNotCorrect;
}

static UDS_HANDLER(handle_transfer_exit)
{
    uds_file_service_t *ctx = (uds_file_service_t *)context;
    if (!ctx) return UDS_NRC_ConditionsNotCorrect;
    
    UDSRequestTransferExitArgs_t *args = (UDSRequestTransferExitArgs_t *)data;
    
    if (ctx->fd < 0) return UDS_NRC_RequestSequenceError;

    /* Integrity Check (Upload Only) */
    if (ctx->mode == FILE_MODE_WRITING) {
        if (args->len >= 4) {
            uint32_t client_crc = 0;
            client_crc |= (uint32_t)args->data[0] << 24;
            client_crc |= (uint32_t)args->data[1] << 16;
            client_crc |= (uint32_t)args->data[2] << 8;
            client_crc |= (uint32_t)args->data[3];

            if (client_crc != ctx->current_crc) {
                LOG_E("CRC32 Mismatch! Serv=0x%X Client=0x%X", ctx->current_crc, client_crc);
                close(ctx->fd);
                unlink(ctx->current_path);
                ctx->fd = -1;
                ctx->mode = FILE_MODE_IDLE;
                return UDS_NRC_GeneralProgrammingFailure; 
            }
        } 
    }
    else if (ctx->mode == FILE_MODE_READING) {
        uint8_t crc_buf[4];
        crc_buf[0] = (uint8_t)((ctx->current_crc >> 24) & 0xFF);
        crc_buf[1] = (uint8_t)((ctx->current_crc >> 16) & 0xFF);
        crc_buf[2] = (uint8_t)((ctx->current_crc >> 8) & 0xFF);
        crc_buf[3] = (uint8_t)((ctx->current_crc) & 0xFF);
        
        ctx->mode = FILE_MODE_IDLE;
        if (args->copyResponse) {
            return args->copyResponse(srv, crc_buf, 4);
        }
    }

    close(ctx->fd);
    ctx->fd = -1;
    ctx->mode = FILE_MODE_IDLE;
    return UDS_PositiveResponse;
}

static UDS_HANDLER(handle_session_timeout)
{
    uds_file_service_t *ctx = (uds_file_service_t *)context;
    if (ctx && ctx->fd >= 0) {
        LOG_W("Session timeout! Closing file: %s", ctx->current_path);
        close(ctx->fd);
        ctx->fd = -1;
        ctx->mode = FILE_MODE_IDLE;
    }
    return RTT_UDS_CONTINUE;
}

/* ==========================================================================
 * Public Registration API
 * ========================================================================== */

rt_err_t rtt_uds_file_service_mount(rtt_uds_env_t *env, uds_file_service_t *svc)
{
    if (!env || !svc) return -RT_EINVAL;

    /* Config Handlers */
    RTT_UDS_SERVICE_NODE_INIT(&svc->req_node,  "file_req",  UDS_EVT_RequestFileTransfer, handle_file_request,  svc, RTT_UDS_PRIO_NORMAL);
    RTT_UDS_SERVICE_NODE_INIT(&svc->data_node, "file_data", UDS_EVT_TransferData,        handle_transfer_data, svc, RTT_UDS_PRIO_NORMAL);
    RTT_UDS_SERVICE_NODE_INIT(&svc->exit_node, "file_exit", UDS_EVT_RequestTransferExit, handle_transfer_exit, svc, RTT_UDS_PRIO_NORMAL);
    RTT_UDS_SERVICE_NODE_INIT(&svc->timeout_node, "file_tmo", UDS_EVT_SessionTimeout,    handle_session_timeout, svc, RTT_UDS_PRIO_HIGHEST);

    /* Register */
    rtt_uds_service_register(env, &svc->req_node);
    rtt_uds_service_register(env, &svc->data_node);
    rtt_uds_service_register(env, &svc->exit_node);
    rtt_uds_service_register(env, &svc->timeout_node);

    return RT_EOK;
}

void rtt_uds_file_service_unmount(uds_file_service_t *svc)
{
    if (!svc) return;
    rtt_uds_service_unregister(&svc->req_node);
    rtt_uds_service_unregister(&svc->data_node);
    rtt_uds_service_unregister(&svc->exit_node);
    rtt_uds_service_unregister(&svc->timeout_node);
}

#endif /* UDS_ENABLE_FILE_SVC */
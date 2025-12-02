/**
 * @file service_0x31_console.c
 * @brief Implementation of UDS Service 0x31 (Remote Console) - Context Based.
 */

#include "rtt_uds_service.h"

#define DBG_TAG "uds.console"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef UDS_ENABLE_CONSOLE_SVC

/* ==========================================================================
 * Configuration
 * ========================================================================== */

#define RID_REMOTE_CONSOLE       0xF000
#define REQUIRED_SEC_LEVEL       0x01

/* ==========================================================================
 * Virtual Device Logic (RT-Thread Device Ops)
 * ========================================================================== */

static rt_err_t vcon_init(rt_device_t dev) { return RT_EOK; }
static rt_err_t vcon_open(rt_device_t dev, rt_uint16_t oflag) { return RT_EOK; }
static rt_err_t vcon_close(rt_device_t dev) { return RT_EOK; }

/**
 * @brief  Virtual Write Implementation.
 * @details Since 'struct rt_device' is the first member of 'uds_console_service_t',
 *          we can safely cast 'dev' back to 'ctx'.
 */
static rt_ssize_t vcon_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    /* [Key] Get context from device handle */
    uds_console_service_t *ctx = (uds_console_service_t *)dev;
    const char *str = (const char *)buffer;

    /* 1. Pass-through to Physical UART */
#ifdef UDS_CONSOLE_PASSTHROUGH
    if (ctx->old_console && ctx->old_console->ops->write)
    {
        ctx->old_console->ops->write(ctx->old_console, pos, buffer, size);
    }
#endif

    /* 2. Capture Logic */
    if (ctx->overflow) return size;

    rt_size_t available = UDS_CONSOLE_BUF_SIZE - ctx->pos - 1;

    if (size <= available)
    {
        rt_memcpy(&ctx->buffer[ctx->pos], str, size);
        ctx->pos += size;
        ctx->buffer[ctx->pos] = '\0';
    }
    else
    {
        /* Overflow handling */
        const char *ovf_msg = "\n[TRUNCATED]\n";
        rt_size_t ovf_len = rt_strlen(ovf_msg);
        rt_size_t write_len = 0;
        
        if (available > ovf_len) 
        {
            write_len = available - ovf_len;
            rt_memcpy(&ctx->buffer[ctx->pos], str, write_len);
            ctx->pos += write_len;
        }
        else if (available < ovf_len)
        {
            rt_size_t backtrack = ovf_len - available;
            if (ctx->pos >= backtrack) ctx->pos -= backtrack;
            else ctx->pos = 0; 
        }

        rt_memcpy(&ctx->buffer[ctx->pos], ovf_msg, ovf_len);
        ctx->pos += ovf_len;
        ctx->buffer[ctx->pos] = '\0';
        ctx->overflow = RT_TRUE;
    }

    return size;
}

const struct rt_device_ops vcon_ops = {
    .init = vcon_init,
    .open = vcon_open,
    .close = vcon_close,
    .write = vcon_write,
};

/* ==========================================================================
 * Console Switching Helpers
 * ========================================================================== */

static rt_err_t capture_start(uds_console_service_t *ctx)
{
    /* 1. Reset Buffer state */
    ctx->pos = 0;
    ctx->overflow = RT_FALSE;
    /* We don't need full memset, just null terminate start */
    ctx->buffer[0] = '\0'; 

    /* 2. Save current console */
    ctx->old_console = rt_console_get_device();

    /* 3. Redirect RT-Thread Console to Virtual Device */
    rt_device_t vdev = rt_device_find(ctx->dev_name);
    if (!vdev) {
        LOG_E("Virtual device %s not registered!", ctx->dev_name);
        return -RT_ERROR;
    }
    rt_console_set_device(ctx->dev_name);

    /* 4. Redirect FinSH */
#ifdef RT_USING_FINSH
    finsh_set_device(ctx->dev_name);
#endif

    /* 5. Ensure Pass-through device is ready */
#ifdef UDS_CONSOLE_PASSTHROUGH
    if (ctx->old_console) {
        /* Re-open original console in stream mode to ensure it accepts data */
        rt_device_open(ctx->old_console, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_STREAM);
    }
#endif 
    return RT_EOK;
}

static void capture_stop(uds_console_service_t *ctx)
{
    /* Restore original console */
    if (ctx->old_console)
    {
        rt_console_set_device(ctx->old_console->parent.name);
#ifdef RT_USING_FINSH
        finsh_set_device(ctx->old_console->parent.name);
#endif
        /* Re-open original device to ensure state consistency */
        rt_device_open(ctx->old_console, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_STREAM);
    }
}

/* ==========================================================================
 * Service Handler
 * ========================================================================== */

static UDS_HANDLER(handle_remote_console)
{
    uds_console_service_t *ctx = (uds_console_service_t *)context;
    if (!ctx) return UDS_NRC_ConditionsNotCorrect;

    UDSRoutineCtrlArgs_t *args = (UDSRoutineCtrlArgs_t *)data;
    char cmd_line[UDS_CONSOLE_CMD_BUF_SIZE];

    /* 1. Session Check */
#ifdef UDS_CONSOLE_REQ_EXT_SESSION
    if (srv->sessionType != UDS_LEV_DS_EXTDS && srv->sessionType != UDS_LEV_DS_PRGS)
        return UDS_NRC_ServiceNotSupportedInActiveSession; 
#endif

    /* 2. Security Check */
#ifdef UDS_CONSOLE_REQ_SECURITY
    if (srv->securityLevel < REQUIRED_SEC_LEVEL)
        return UDS_NRC_SecurityAccessDenied;
#endif

    /* 3. Validate Request */
    if (args->ctrlType != UDS_LEV_RCTP_STR)
        return UDS_NRC_SubFunctionNotSupported;

    if (args->id != RID_REMOTE_CONSOLE)
        return UDS_NRC_RequestOutOfRange;

    if (args->len == 0 || args->len >= sizeof(cmd_line))
        return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;

    /* 4. Parse Command */
    rt_memcpy(cmd_line, args->optionRecord, args->len);
    cmd_line[args->len] = '\0'; 

    LOG_D("Remote Exec: %s", cmd_line);

    /* 5. Start Capture */
    if (capture_start(ctx) != RT_EOK) {
        return UDS_NRC_ConditionsNotCorrect;
    }

    /* Echo command to capture buffer for context */
    rt_kprintf("> %s\n", cmd_line); 

    /* Execute MSH Command */
    extern int msh_exec(char *cmd, rt_size_t length);
    msh_exec(cmd_line, rt_strlen(cmd_line));

    /* 6. Stop Capture (Buffer remains valid in ctx) */
    capture_stop(ctx);

    /* 7. Send Response */
    if (args->copyStatusRecord)
    {
        /* Send buffer content */
        return args->copyStatusRecord(srv, (uint8_t *)ctx->buffer, (uint16_t)ctx->pos);
    }

    return UDS_PositiveResponse;
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

rt_err_t rtt_uds_console_service_mount(rtt_uds_env_t *env, uds_console_service_t *svc)
{
    if (!env || !svc) return -RT_EINVAL;

    /* 1. Register Virtual Device to RT-Thread */
    /* Init device structure */
    svc->dev.type = RT_Device_Class_Char;
    svc->dev.ops  = &vcon_ops;
    /* user_data point to self (though we cast 'dev' pointer, this is good practice) */
    svc->dev.user_data = svc; 

    /* Use configured name or default */
    const char *dev_name = svc->dev_name ? svc->dev_name : "uds_vcon";
    
    if (rt_device_register(&svc->dev, dev_name, RT_DEVICE_FLAG_RDWR) != RT_EOK) {
        LOG_E("Failed to register virtual device %s", dev_name);
        return -RT_ERROR;
    }

    /* 2. Configure UDS Handler */
    RTT_UDS_SERVICE_NODE_INIT(&svc->service_node,
                              "console_exec",
                              UDS_EVT_RoutineCtrl,
                              handle_remote_console,
                              svc, /* Context binding */
                              RTT_UDS_PRIO_NORMAL);

    /* 3. Register to UDS Core */
    return rtt_uds_service_register(env, &svc->service_node);
}

void rtt_uds_console_service_unmount(uds_console_service_t *svc)
{
    if (!svc) return;

    /* Unregister from UDS */
    rtt_uds_service_unregister(&svc->service_node);

    /* Unregister from RT-Thread */
    rt_device_unregister(&svc->dev);
    
    LOG_I("Console Service Unmounted");
}

#endif /* UDS_ENABLE_CONSOLE_SVC */
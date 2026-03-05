/**
 * @file rtt_uds_0x2a_ulog_example.c
 * @brief Example adapter: stream RT-Thread ULOG through UDS service 0x2A.
 * @details This file is an example-only integration that binds ULOG output to a
 *          provider in service_0x2A_periodic.c. It keeps only two public entry
 *          points (mount/unmount). No public header is provided intentionally.
 *
 * @note This example protects ringbuffer access with one mutex and assumes this
 *       backend runs in thread context only. If future use includes ISR context,
 *       replace mutex protection with spin_lock_irqsave style synchronization.
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2026-02-22
 * 
 * @copyright Copyright (c) 2026  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026-02-22 1.0     wdfk-prog   first version
 */

#include "rtt_uds_service.h"

#if defined(UDS_ENABLE_0X2A_PERIODIC_SVC) && defined(RT_USING_ULOG)

#include <ipc/ringbuffer.h>
#include <rtatomic.h>
#include <ulog.h>

/**
 * @brief Default PDID used by this example provider.
 */
#ifndef UDS_0X2A_ULOG_EXAMPLE_PDID
#define UDS_0X2A_ULOG_EXAMPLE_PDID 0xA1
#endif

/**
 * @brief Ring buffer capacity in bytes for captured ULOG stream.
 */
#ifndef UDS_0X2A_ULOG_EXAMPLE_RINGBUF_SIZE
#define UDS_0X2A_ULOG_EXAMPLE_RINGBUF_SIZE 1024
#endif

/**
 * @brief Single blacklist tag ignored by this example.
 * @details Logs with this tag are filtered out before entering 0x2A stream.
 */
#ifndef UDS_0X2A_ULOG_EXAMPLE_IGNORE_TAG
#define UDS_0X2A_ULOG_EXAMPLE_IGNORE_TAG "uds.console"
#endif

#if (UDS_0X2A_ULOG_EXAMPLE_RINGBUF_SIZE < 1)
#error "UDS_0X2A_ULOG_EXAMPLE_RINGBUF_SIZE must be >= 1"
#endif

/**
 * @brief Runtime context of the ULOG->0x2A example bridge.
 */
typedef struct
{
    uds_0x2a_service_t periodic_svc; /**< 0x2A business adapter context */
    uds_0x2a_provider_t provider;    /**< one PDID provider used by this example */
    struct ulog_backend backend;     /**< dedicated ULOG backend instance */
    struct rt_ringbuffer rb;         /**< byte stream queue for captured logs */
    rt_uint8_t rb_pool[UDS_0X2A_ULOG_EXAMPLE_RINGBUF_SIZE]; /**< static ringbuffer storage */
    struct rt_mutex rb_lock;         /**< mutex protecting ringbuffer put/get/reset */
    rt_atomic_t enabled; /**< internal enable flag: 0=disabled/unmounted, nonzero=enabled */
} uds_0x2a_ulog_example_ctx_t;

/**
 * @brief Global example context.
 */
static uds_0x2a_ulog_example_ctx_t g_ulog_2a = { 0 };

/**
 * @brief Push one ULOG record into ringbuffer with overwrite behavior.
 * @details rt_ringbuffer_put_force() overwrites oldest bytes when space is insufficient.
 *
 * @param ctx Example context.
 * @param log Pointer to formatted ULOG bytes.
 * @param len Length of @p log in bytes.
 */
static void ulog_stream_put_force(uds_0x2a_ulog_example_ctx_t *ctx, const char *log, rt_size_t len)
{
    rt_err_t lock_ret;
    rt_uint32_t rb_size;
    rt_uint32_t space_before;
    rt_size_t written;
    rt_bool_t warn_overwrite;

    if (ctx == RT_NULL || log == RT_NULL || len == 0U)
    {
        return;
    }

    lock_ret = rt_mutex_take(&ctx->rb_lock, RT_WAITING_FOREVER);
    if (lock_ret != RT_EOK)
    {
        return;
    }

    rb_size = rt_ringbuffer_get_size(&ctx->rb);
    space_before = rt_ringbuffer_space_len(&ctx->rb);

    written = rt_ringbuffer_put_force(&ctx->rb, (const rt_uint8_t *)log, len);
    warn_overwrite = (written < len) ? RT_TRUE : RT_FALSE;

    rt_mutex_release(&ctx->rb_lock);

    if (warn_overwrite == RT_TRUE)
    {
        rt_kprintf("[uds.0x2a.ulog] ringbuffer overwrite/truncate (in=%u, wrote=%u, space=%u, size=%u)\n",
                   (unsigned int)len,
                   (unsigned int)written,
                   (unsigned int)space_before,
                   (unsigned int)rb_size);
    }
}

/**
 * @brief ULOG backend filter callback.
 * @details Accepts logs only when enabled and tag is not blacklisted.
 *
 * @param backend Backend instance (unused).
 * @param level Log level (unused).
 * @param tag ULOG tag.
 * @param is_raw Raw flag (unused).
 * @param log Log text pointer (unused by filter decision).
 * @param len Log text length (unused by filter decision).
 * @return RT_TRUE when the log should be sent to backend output.
 */
static rt_bool_t uds_0x2a_ulog_filter(struct ulog_backend *backend,
                                      rt_uint32_t level,
                                      const char *tag,
                                      rt_bool_t is_raw,
                                      const char *log,
                                      rt_size_t len)
{
    (void)backend;
    (void)level;
    (void)is_raw;
    (void)log;
    (void)len;

    if (rt_atomic_load(&g_ulog_2a.enabled) == 0)
    {
        return RT_FALSE;
    }

    if (tag != RT_NULL &&
        rt_strncmp(tag, UDS_0X2A_ULOG_EXAMPLE_IGNORE_TAG, ULOG_FILTER_TAG_MAX_LEN) == 0)
    {
        return RT_FALSE;
    }

    return RT_TRUE;
}

/**
 * @brief ULOG backend output callback.
 * @details Captures accepted ULOG records into byte-stream ringbuffer.
 *
 * @param backend Backend instance (unused).
 * @param level Log level (unused).
 * @param tag ULOG tag (unused).
 * @param is_raw Raw flag (unused).
 * @param log Pointer to formatted log text.
 * @param len Length of @p log.
 */
static void uds_0x2a_ulog_output(struct ulog_backend *backend,
                                 rt_uint32_t level,
                                 const char *tag,
                                 rt_bool_t is_raw,
                                 const char *log,
                                 rt_size_t len)
{
    (void)backend;
    (void)level;
    (void)tag;
    (void)is_raw;

    if (rt_atomic_load(&g_ulog_2a.enabled) == 0)
    {
        return;
    }

    ulog_stream_put_force(&g_ulog_2a, log, len);
}

/**
 * @brief 0x2A provider read callback.
 * @details Reads bytes from ringbuffer into transmit payload.
 *
 * @param srv UDS server instance (unused).
 * @param data_id PDID (unused).
 * @param transmission_mode Active transmission mode (unused).
 * @param payload Output payload buffer provided by core.
 * @param context User context (unused).
 * @return UDS_PositiveResponse when at least one byte is produced, otherwise
 *         UDS_NRC_RequestOutOfRange.
 */
static UDSErr_t ulog_provider_read(UDSServer_t *srv,
                                   uint8_t data_id,
                                   UDSRDBPITransmissionMode_t transmission_mode,
                                   UDSPayloadBuffer_t *payload,
                                   void *context)
{
    rt_err_t lock_ret;
    rt_size_t read_len;

    (void)srv;
    (void)data_id;
    (void)transmission_mode;
    (void)context;

    if (rt_atomic_load(&g_ulog_2a.enabled) == 0)
    {
        return UDS_NRC_RequestOutOfRange;
    }

    lock_ret = rt_mutex_take(&g_ulog_2a.rb_lock, RT_WAITING_FOREVER);
    if (lock_ret != RT_EOK)
    {
        return UDS_NRC_ConditionsNotCorrect;
    }

    read_len = rt_ringbuffer_get(&g_ulog_2a.rb, payload->data, payload->bufLen);
    payload->len = (uint16_t)read_len;
    rt_mutex_release(&g_ulog_2a.rb_lock);

    if (read_len == 0U)
    {
        return UDS_NRC_RequestOutOfRange;
    }

    return UDS_PositiveResponse;
}

/**
 * @brief Mount ULOG->0x2A example bridge.
 * @details Initializes provider, mounts 0x2A service nodes and registers ULOG backend.
 *
 * @param env UDS runtime environment.
 * @return RT_EOK on success, otherwise RT-Thread error code.
 */
rt_err_t uds_0x2a_ulog_example_mount(rtt_uds_env_t *env)
{
    rt_err_t ret;

    if (env == RT_NULL)
    {
        return -RT_EINVAL;
    }

    if (rt_atomic_load(&g_ulog_2a.enabled) != 0)
    {
        return RT_EOK;
    }

    rt_memset(&g_ulog_2a, 0, sizeof(g_ulog_2a));
    RTT_UDS_0X2A_SERVICE_INIT(&g_ulog_2a.periodic_svc, "ulog_2a");
    ret = rt_mutex_init(&g_ulog_2a.rb_lock, "u2a_rb", RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK)
    {
        return ret;
    }
    rt_ringbuffer_init(&g_ulog_2a.rb, g_ulog_2a.rb_pool, UDS_0X2A_ULOG_EXAMPLE_RINGBUF_SIZE);

    g_ulog_2a.provider.data_id = UDS_0X2A_ULOG_EXAMPLE_PDID;
    g_ulog_2a.provider.check = RT_NULL;
    g_ulog_2a.provider.read = ulog_provider_read;
    g_ulog_2a.provider.context = RT_NULL;
    g_ulog_2a.provider.subscribed = RT_FALSE;

    ret = uds_0x2a_register_provider(&g_ulog_2a.periodic_svc, &g_ulog_2a.provider);
    if (ret != RT_EOK)
    {
        goto __exit_lock;
    }

    ret = rtt_uds_0x2a_service_mount(env, &g_ulog_2a.periodic_svc);
    if (ret != RT_EOK)
    {
        goto __exit_provider;
    }

    g_ulog_2a.backend.output = uds_0x2a_ulog_output;
    g_ulog_2a.backend.filter = uds_0x2a_ulog_filter;

    ret = ulog_backend_register(&g_ulog_2a.backend, "uds2a_ulog", RT_FALSE);
    if (ret != RT_EOK)
    {
        goto __exit_service;
    }

    rt_atomic_store(&g_ulog_2a.enabled, 1);
    return RT_EOK;

__exit_service:
    rtt_uds_0x2a_service_unmount(&g_ulog_2a.periodic_svc);
__exit_provider:
    uds_0x2a_unregister_provider(&g_ulog_2a.periodic_svc, g_ulog_2a.provider.data_id);
__exit_lock:
    rt_mutex_detach(&g_ulog_2a.rb_lock);
    return ret;
}

/**
 * @brief Unmount ULOG->0x2A example bridge.
 * @details Unregisters ULOG backend and unmounts 0x2A service nodes.
 */
void uds_0x2a_ulog_example_unmount(void)
{
    rt_err_t lock_ret;

    if (rt_atomic_load(&g_ulog_2a.enabled) == 0)
    {
        return;
    }

    rt_atomic_store(&g_ulog_2a.enabled, 0);
    ulog_backend_unregister(&g_ulog_2a.backend);

    lock_ret = rt_mutex_take(&g_ulog_2a.rb_lock, RT_WAITING_FOREVER);
    if (lock_ret == RT_EOK)
    {
        rt_ringbuffer_reset(&g_ulog_2a.rb);
        rt_mutex_release(&g_ulog_2a.rb_lock);
    }

    rtt_uds_0x2a_service_unmount(&g_ulog_2a.periodic_svc);
    uds_0x2a_unregister_provider(&g_ulog_2a.periodic_svc, g_ulog_2a.provider.data_id);
    rt_mutex_detach(&g_ulog_2a.rb_lock);
}

#endif /* UDS_ENABLE_0X2A_PERIODIC_SVC && RT_USING_ULOG */

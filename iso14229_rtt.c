/**
 * @file iso14229_rtt.c
 * @brief Implementation of the RT-Thread UDS (ISO14229) porting layer.
 * @details This file implements the glue logic between the generic ISO14229 library,
 *          the ISO-TP transport layer, and the RT-Thread operating system features
 *          (Threads, IPC, Hardware Drivers).
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2025-11-19
 * 
 * @copyright Copyright (c) 2025
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2025-11-19 1.0     wdfk-prog   first version
 */
#include "iso14229_rtt.h"
#include <stdio.h>

#define DBG_TAG "uds.rtt"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/**
 * @brief Internal UDS Environment Control Block.
 * @details Management structure containing the core server instance, transport layer,
 *          RT-Thread resources (thread, mq, device), and the event dispatch table.
 */
struct rtt_uds_env
{
    UDSServer_t server;         /**< Core UDS Server instance */
    UDSISOTpC_t tp;             /**< ISO-TP Transport Layer instance */

    rt_device_t can_dev;        /**< Handle to CAN device (used for Transmission) */
    rt_mq_t rx_mq;              /**< Message Queue for buffering incoming CAN frames */
    rt_thread_t thread;         /**< Main processing thread handle */

    /** 
     * @brief Event Dispatch Table.
     * @details An array of linked lists indexed by the UDS Event ID (SID).
     *          Allows O(1) lookup complexity for event dispatching.
     */
    rt_list_t event_table[UDS_RTT_EVENT_TABLE_SIZE];

    rtt_uds_config_t config;    /**< Local copy of configuration parameters */
};

/* ==========================================================================
 * Utility & Logging Implementation
 * ========================================================================== */

/**
 * @brief  Log data in hexadecimal format.
 * @note   Only outputs logs if DBG_LVL is set to DBG_LOG or higher.
 * 
 * @param  title Description string printed before the data.
 * @param  data  Pointer to the data buffer.
 * @param  size  Size of the data in bytes.
 */
void rtt_uds_log_hex(const char *title, const uint8_t *data, rt_size_t size)
{
    (void)title;
    (void)data;
    (void)size;
#if (DBG_LVL >= DBG_LOG)
    char log_buf[256];
    int offset = 0;

    offset += rt_snprintf(log_buf + offset, sizeof(log_buf) - offset, "%s [%d bytes]:", title, size);

    for (uint16_t i = 0; i < size; i++)
    {
        /* Prevent buffer overflow */
        if (offset >= sizeof(log_buf) - 4)
        {
            rt_snprintf(log_buf + offset, sizeof(log_buf) - offset, " ...");
            break;
        }
        offset += rt_snprintf(log_buf + offset, sizeof(log_buf) - offset, " %02X", data[i]);
    }

    LOG_D("%s", log_buf);
#endif
}

/**
 * @brief  Debug output callback required by the ISO-TP library.
 * @details Adapts the library's printf-style logging to RT-Thread's ULOG or kprintf.
 * 
 * @param  format Printf-style format string.
 * @param  ...    Variable arguments.
 */
void isotp_user_debug(const char *format, ...)
{
    va_list args;
    va_start(args, format);

#if defined(RT_USING_ULOG) && defined(ULOG_BACKEND_USING_CONSOLE)
    /* Forward to ULOG if available */
    ulog_voutput(DBG_LVL, DBG_TAG, RT_TRUE, RT_NULL, 0, 0, 0, format, args);
#else
    /* Fallback to standard kernel printf */
    rt_kprintf("[%s] ", DBG_TAG);
    rt_vprintf(format, args);
    rt_kprintf("\n");
#endif

    va_end(args);
}

/* ==========================================================================
 * ISO-TP Adapter Logic
 * ========================================================================== */

/**
 * @brief  Hardware send callback required by the ISO-TP library.
 * @details Writes a CAN frame to the underlying RT-Thread CAN device.
 * 
 * @param  arbitration_id CAN ID for the frame (11-bit or 29-bit).
 * @param  data           Pointer to payload data.
 * @param  size           Size of payload (0-8 bytes).
 * @param  user_data      User context (passed as rt_device_t).
 * @return ISOTP_RET_OK on success, ISOTP_RET_ERROR on failure.
 */
int isotp_user_send_can(const uint32_t arbitration_id, const uint8_t *data, const uint8_t size, void *user_data)
{
    rt_device_t dev = (rt_device_t)user_data;
    struct rt_can_msg msg = { 0 };

    if (dev == RT_NULL)
    {
        return ISOTP_RET_ERROR;
    }

    /* Construct CAN Message */
    msg.id = arbitration_id;
    msg.ide = RT_CAN_STDID; /* Default to Standard ID. Modify if Extended ID is needed. */
    msg.rtr = RT_CAN_DTR;   /* Data Frame */
    msg.len = size;
    rt_memcpy(msg.data, data, size);

#if (DBG_LVL >= DBG_LOG)
    char title_buf[32];
    rt_snprintf(title_buf, sizeof(title_buf), "[TX] ID: 0x%lX", arbitration_id);
    rtt_uds_log_hex(title_buf, msg.data, size);
#endif

    rt_size_t written = rt_device_write(dev, 0, &msg, sizeof(msg));

    if (written != sizeof(msg))
    {
        LOG_E("CAN write failed! Written: %d, Expected: %d", written, sizeof(msg));
        return ISOTP_RET_ERROR;
    }

    return ISOTP_RET_OK;
}

/**
 * @brief  Get current system time in microseconds.
 * @details Used by ISO-TP library for timing constraints (N_As, N_Bs, etc.).
 * @return System time in microseconds.
 */
uint32_t isotp_user_get_us(void)
{
    return (uint32_t)((rt_uint64_t)rt_tick_get() * 1000000 / RT_TICK_PER_SECOND);
}

/* ==========================================================================
 * Core Server Logic
 * ========================================================================== */

/**
 * @brief  Central Event Dispatcher (Router).
 * @details Implements the "Chain of Responsibility" pattern. It looks up the event list
 *          in the O(1) table and iterates through registered handlers based on priority.
 * 
 * @param  srv  Pointer to the UDS server instance.
 * @param  evt  The event ID (UDSEvent_t) to dispatch.
 * @param  data Event-specific argument structure (e.g., UDSRDBIArgs_t*).
 * @return UDSErr_t UDS_PositiveResponse if handled, otherwise an appropriate NRC.
 */
static UDSErr_t server_event_dispatcher(UDSServer_t *srv, UDSEvent_t evt, void *data)
{
    rtt_uds_env_t *env = (rtt_uds_env_t *)srv->fn_data;
    uds_service_node_t *node;
    UDSErr_t final_result = UDS_NRC_ServiceNotSupported;

    /* 1. Safety check for array bounds */
    if (evt >= UDS_RTT_EVENT_TABLE_SIZE)
    {
        LOG_E("Event ID %d out of range! Max is %d", evt, UDS_RTT_EVENT_TABLE_SIZE);
        return UDS_NRC_GeneralReject;
    }

    LOG_D("Dispatch Event: %s (0x%X)", UDSEventToStr(evt), evt);

    /* 2. Get the list head for this specific event (O(1) lookup) */
    rt_list_t *head = &env->event_table[evt];

    /* If list is empty, no handler is registered for this SID */
    if (rt_list_isempty(head))
    {
        return UDS_NRC_ServiceNotSupported;
    }

    /* 3. Iterate through registered handlers (Chain of Responsibility) */
    rt_list_for_each_entry(node, head, list)
    {
        if (node->handler)
        {
            UDSErr_t result = node->handler(srv, data, node->context);

            /* 
             * Scenario A: Broadcaster / Observer Pattern
             * If handler returns RTT_UDS_CONTINUE, it means "I processed it, but let others process it too".
             * This is useful for logging, status updates, or reset hooks.
             */
            if (result == RTT_UDS_CONTINUE)
            {
                final_result = UDS_PositiveResponse; /* Mark as handled at least once */
                continue;
            }

            /* 
             * Scenario B: Request Handled Successfully
             * Stop the chain. Return success to the core library.
             */
            if (result == UDS_PositiveResponse ||
                result == UDS_NRC_RequestCorrectlyReceived_ResponsePending)
            {
                return result;
            }

            /* 
             * Scenario C: Not My Responsibility
             * Handler checked the DID/SubFunction and it doesn't match.
             * Continue to the next handler in the chain.
             */
            if (result == UDS_NRC_RequestOutOfRange ||
                result == UDS_NRC_SubFunctionNotSupported)
            {
                continue;
            }

            /* 
             * Scenario D: Critical Failure / Rejection
             * Handler matched the request but rejected it (e.g., Security Access Denied, Conditions Not Correct).
             * Stop the chain and report the error immediately.
             */
            return result;
        }
    }

    /* 
     * End of Chain Reached:
     * - If at least one handler returned RTT_UDS_CONTINUE, final_result is PositiveResponse.
     * - If no handler matched (all returned NotSupported/OutOfRange), final_result is ServiceNotSupported.
     */
    return final_result;
}

/**
 * @brief  Main UDS processing thread entry point.
 * @details Handles CAN reception via Message Queue and polls the UDS/ISO-TP stacks.
 *          Implements a dynamic timeout strategy to balance performance and CPU usage.
 * 
 * @param  parameter Pointer to the rtt_uds_env_t instance.
 */
static void uds_thread_entry(void *parameter)
{
    rtt_uds_env_t *env = (rtt_uds_env_t *)parameter;
    struct rt_can_msg rx_msg;
    rt_int32_t timeout;

    while (1)
    {
        /* 
         * [Optimization] Dynamic Timeout Strategy
         * Check if ISO-TP layer is currently transmitting a multi-frame message.
         * - If Transmitting (INPROGRESS): Do not block. Poll immediately to keep CAN bus full.
         * - If Idle: Block for 10ms to yield CPU to other threads.
         */
        if (env->tp.phys_link.send_status == ISOTP_SEND_STATUS_INPROGRESS ||
            env->tp.func_link.send_status == ISOTP_SEND_STATUS_INPROGRESS)
        {
            timeout = RT_WAITING_NO; /* Non-blocking polling */
        }
        else
        {
            timeout = rt_tick_from_millisecond(10); /* Sleep 10ms */
        }

        /* Wait for incoming CAN frames from the RX Callback */
        rt_ssize_t ret = rt_mq_recv(env->rx_mq, &rx_msg, sizeof(struct rt_can_msg), timeout);
        if (ret == sizeof(struct rt_can_msg))
        {
#if (DBG_LVL >= DBG_LOG)
            char title[32];
            rt_snprintf(title, sizeof(title), "CAN RX ID:0x%lX", rx_msg.id);
            rtt_uds_log_hex(title, rx_msg.data, rx_msg.len);
#endif
            /* Dispatch frame to ISO-TP layer based on ID */
            if (rx_msg.id == env->tp.phys_sa)
            {
                /* Physical Addressing (1:1) */
                isotp_on_can_message(&env->tp.phys_link, rx_msg.data, rx_msg.len);
            }
            else if (rx_msg.id == env->tp.func_sa)
            {
                /* Functional Addressing (Broadcast) */
                /* ISO-15765 Rule: Ignore functional requests if a physical segmented transfer is active */
                if (ISOTP_RECEIVE_STATUS_IDLE != env->tp.phys_link.receive_status)
                {
                    LOG_W("Dropped Functional frame: Physical link is busy.");
                    continue;
                }
                isotp_on_can_message(&env->tp.func_link, rx_msg.data, rx_msg.len);
            }
            else
            {
                LOG_D("Received irrelevant CAN ID 0x%03lX", rx_msg.id);
            }
        }
        else if (ret != -RT_ETIMEOUT)
        {
            /* Log unexpected errors (e.g., -RT_ERROR, -RT_EIO) */
            /* -RT_ETIMEOUT is normal behavior in this loop */
            LOG_E("MQ receive error: %d", ret);
        }

        /* Run the UDS Server State Machine */
        UDSServerPoll(&env->server);

        /* 
        * [Optimization] Yield in high-load scenarios
        * If running in non-blocking mode and no message was received, 
        * explicitly yield to prevent starving lower priority threads completely.
        */
        if (timeout == RT_WAITING_NO)
        {
            rt_thread_yield();
        }
    }
}

/* ==========================================================================
 * Public API Implementation
 * ========================================================================== */

/**
 * @brief Helper: Check if TX is allowed based on ControlType
 */
static int is_tx_allowed(uint8_t ctrl_type)
{
    /* 0x00: EnableRxTx, 0x02: DisableRxEnableTx -> TX Allowed */
    return (ctrl_type == UDS_LEV_CTRLTP_ERXTX || ctrl_type == UDS_LEV_CTRLTP_DRXETX);
}

/**
 * @brief Helper: Check if RX is allowed based on ControlType
 */
static int is_rx_allowed(uint8_t ctrl_type)
{
    /* 0x00: EnableRxTx, 0x01: EnableRxDisableTx -> RX Allowed */
    return (ctrl_type == UDS_LEV_CTRLTP_ERXTX || ctrl_type == UDS_LEV_CTRLTP_ERXDTX);
}

/* --- Communication Control API: Normal Messages (Application Data) --- */

int rtt_uds_is_app_tx_enabled(rtt_uds_env_t *env)
{
    if (!env)
        return 1; /* Default to enabled if UDS not running */
    return is_tx_allowed(env->server.commState_Normal);
}

int rtt_uds_is_app_rx_enabled(rtt_uds_env_t *env)
{
    if (!env)
        return 1;
    return is_rx_allowed(env->server.commState_Normal);
}

/* --- Communication Control API: NM Messages (Network Management) --- */

int rtt_uds_is_nm_tx_enabled(rtt_uds_env_t *env)
{
    if (!env)
        return 1;
    return is_tx_allowed(env->server.commState_NM);
}

int rtt_uds_is_nm_rx_enabled(rtt_uds_env_t *env)
{
    if (!env)
        return 1;
    return is_rx_allowed(env->server.commState_NM);
}

/**
 * @brief  Feed a CAN frame into the UDS stack's message queue.
 * @note   This function is non-blocking and safe to call from ISR or CAN callback.
 * 
 * @param  env Pointer to the UDS environment handle.
 * @param  msg Pointer to the received CAN message.
 * @return RT_EOK on success, -RT_ERROR if env is invalid, error code from rt_mq_send otherwise.
 */
rt_err_t rtt_uds_feed_can_frame(rtt_uds_env_t *env, struct rt_can_msg *msg)
{
    if (!env || !env->rx_mq)
        return -RT_ERROR;

    /* Put message into queue. Do not block if full (wait time 0). */
    rt_err_t ret = rt_mq_send(env->rx_mq, msg, sizeof(struct rt_can_msg));

    if (ret != RT_EOK)
    {
        /* -RT_EFULL indicates the queue is full (CPU overloaded or thread stuck) */
        LOG_E("Feed CAN frame failed! Queue full? Error: %d", ret);
    }

    return ret;
}

/**
 * @brief  Register a service handler for a specific UDS event.
 * @details Inserts the node into the event chain based on priority (0 is highest).
 * 
 * @param  env  Pointer to the UDS environment handle.
 * @param  node Pointer to the static service node to register.
 * @return RT_EOK on success, -RT_EINVAL for invalid args, -RT_EBUSY if already registered.
 */
rt_err_t rtt_uds_service_register(rtt_uds_env_t *env, uds_service_node_t *node)
{
    if (!env || !node)
        return -RT_EINVAL;

    if (node->event >= UDS_RTT_EVENT_TABLE_SIZE)
    {
        LOG_E("Event ID %s exceeds table size", UDSEventToStr(node->event));
        return -RT_EINVAL;
    }

    if (!rt_list_isempty(&node->list))
    {
        LOG_W("Service event %s already registered", UDSEventToStr(node->event));
        return -RT_EBUSY;
    }

    /* Get the list head for this Event ID */
    rt_list_t *head = &env->event_table[node->event];
    uds_service_node_t *curr;

    /* Iterate to find insertion point based on Priority */
    rt_list_for_each_entry(curr, head, list)
    {
        /* 
         * Priority Sort: Ascending Order (0, 1, 2...)
         * If new node's priority number is smaller than current node's,
         * insert it BEFORE current node.
         */
        if (node->priority < curr->priority)
        {
            rt_list_insert_before(&curr->list, &node->list);
            return RT_EOK;
        }
    }

    /* If list is empty or new node has lowest priority (highest number), insert at end */
    rt_list_insert_before(head, &node->list);

    return RT_EOK;
}

/**
 * @brief  Unregister a previously registered service handler.
 * @param  node Pointer to the service node to remove.
 */
void rtt_uds_service_unregister(uds_service_node_t *node)
{
    if (!node)
        return;

    if (!rt_list_isempty(&node->list))
    {
        rt_list_remove(&node->list);
        rt_list_init(&node->list); /* Mark as detached */
    }
    LOG_D("Service %s unregistered.", node->name ? node->name : "Unknown");
}

/**
 * @brief  Unregister ALL service handlers from the environment.
 * @param  env Pointer to the UDS environment handle.
 */
void rtt_uds_service_unregister_all(rtt_uds_env_t *env)
{
    if (!env)
        return;

    /* Iterate through the entire event table */
    for (int i = 0; i < UDS_RTT_EVENT_TABLE_SIZE; i++)
    {
        rt_list_t *head = &env->event_table[i];

        /* Safely remove all nodes in the list */
        while (!rt_list_isempty(head))
        {
            /* Get first node */
            uds_service_node_t *node = rt_list_entry(head->next, uds_service_node_t, list);

            /* Unregister it */
            rtt_uds_service_unregister(node);
        }
    }

    LOG_I("All UDS services unregistered.");
}

/**
 * @brief  Destroy a UDS service instance.
 * @details Stops thread, deletes resources, and frees memory.
 *          Safe to call on partially initialized instances (handles must be NULL if invalid).
 * @param  env Pointer to the UDS environment handle.
 */
void rtt_uds_destroy(rtt_uds_env_t *env)
{
    rt_err_t err;
    if (!env)
        return;

    /* Delete thread if it exists */
    if (env->thread != RT_NULL)
    {
        err = rt_thread_delete(env->thread);
        if (err != RT_EOK)
        {
            LOG_E("Failed to delete thread. Error: %d", err);
        }
    }

    /* Delete MQ if it exists */
    if (env->rx_mq != RT_NULL)
    {
        err = rt_mq_delete(env->rx_mq);
        if (err != RT_EOK)
        {
            LOG_E("Failed to delete MQ. Error: %d", err);
        }
    }

    /* Free main structure */
    rt_free(env);
}

/**
 * @brief  Create and initialize a UDS service instance.
 * @details Allocates memory, initializes ISO-TP, creates MQ and Thread, and starts the thread.
 * 
 * @param  cfg Pointer to configuration structure.
 * @return Pointer to new instance handle, or RT_NULL on failure.
 */
rtt_uds_env_t *rtt_uds_create(const rtt_uds_config_t *cfg)
{
    rt_err_t err = RT_EOK;
    
    /* 1. Allocate and Zero-init (Crucial for cleanup safety) */
    rtt_uds_env_t *env = rt_malloc(sizeof(rtt_uds_env_t));
    if (!env)
    {
        LOG_E("Failed to allocate UDS environment memory");
        return RT_NULL;
    }
    rt_memset(env, 0, sizeof(rtt_uds_env_t));
    
    /* 2. Basic Config Copy */
    env->config = *cfg;

    /* 3. Initialize Lists */
    for (int i = 0; i < UDS_RTT_EVENT_TABLE_SIZE; i++)
    {
        rt_list_init(&env->event_table[i]);
    }

    /* 4. Find Hardware Device */
    env->can_dev = rt_device_find(cfg->can_name);
    if (!env->can_dev)
    {
        LOG_E("CAN device %s not found", cfg->can_name);
        goto __exit_error;
    }

    /* 5. Initialize ISO-TP Layer */
    UDSISOTpCConfig_t tp_cfg = {
        .source_addr = cfg->phys_id,
        .target_addr = cfg->resp_id,
        .source_addr_func = cfg->func_id,
        .target_addr_func = cfg->func_resp_id
    };
    UDSISOTpCInit(&env->tp, &tp_cfg);

    /* Bind Device to ISO-TP User Args */
    env->tp.phys_link.user_send_can_arg = env->can_dev;
    env->tp.func_link.user_send_can_arg = env->can_dev;

    /* 6. Initialize Core UDS Server */
    UDSServerInit(&env->server);
    env->server.tp = &env->tp.hdl;
    env->server.fn_data = env;
    env->server.fn = server_event_dispatcher;

    /* 7. Create Message Queue */
    char mq_name[RT_NAME_MAX];
    rt_snprintf(mq_name, sizeof(mq_name), "%s_uds_mq", cfg->can_name);
    rt_size_t pool_size = cfg->rx_mq_pool_size > 0 ? cfg->rx_mq_pool_size : 32;

    env->rx_mq = rt_mq_create(mq_name, sizeof(struct rt_can_msg), pool_size, RT_IPC_FLAG_FIFO);
    if (!env->rx_mq)
    {
        LOG_E("MQ create failed");
        goto __exit_error;
    }

    /* 8. Create Thread */
    env->thread = rt_thread_create(cfg->thread_name, uds_thread_entry, env,
                                   cfg->stack_size, cfg->priority, 10);
    if (!env->thread)
    {
        LOG_E("Thread create failed");
        goto __exit_error;
    }

    /* 9. Start Thread */
    err = rt_thread_startup(env->thread);
    if (err != RT_EOK)
    {
        LOG_E("Thread startup failed! Error: %d", err);
        goto __exit_error;
    }

    return env;

__exit_error:
    /* 
     * Unified cleanup:
     * rtt_uds_destroy checks for NULL handles before deletion.
     * Since we memset(0) at the start, uncreated resources are NULL and safe.
     */
    rtt_uds_destroy(env);
    return RT_NULL;
}

/* ==========================================================================
 * Default Logging & Debug Features
 * ========================================================================== */

/**
 * @brief Default Handler for Session Timeout Logging.
 * @details Registered with high priority to log when a session times out to default.
 */
static UDS_HANDLER(handle_general_log_timeout)
{
    /* The core library has already reset srv->sessionType to Default (0x01) */
    LOG_W("Session Timeout! Resetting to Default Session.");

    /* Return CONTINUE to allow other handlers (e.g., IO Reset) to execute */
    return RTT_UDS_CONTINUE;
}

static uds_service_node_t log_timeout_node = {
    .list = RT_LIST_OBJECT_INIT(log_timeout_node.list),
    .name = "sys_log_timeout",
    .event = UDS_EVT_SessionTimeout,
    .handler = handle_general_log_timeout,
    .priority = RTT_UDS_PRIO_HIGHEST, /* Highest priority to log first */
    .context = RT_NULL
};

void log_timeout_node_register(rtt_uds_env_t *env)
{
    if (env)
    {
        rtt_uds_service_register(env, &log_timeout_node);
    }
}

static const char *get_session_name(uint8_t type)
{
    switch (type)
    {
    case UDS_LEV_DS_DS:
        return "Default (0x01)";
    case UDS_LEV_DS_PRGS:
        return "Programming (0x02)";
    case UDS_LEV_DS_EXTDS:
        return "Extended (0x03)";
    case UDS_LEV_DS_SSDS:
        return "SafetySystem (0x04)";
    default:
        return "Unknown";
    }
}

static const char *get_comm_ctrl_name(uint8_t type)
{
    switch (type)
    {
    case 0x00:
        return "EnableRxTx (Normal)";
    case 0x01:
        return "EnableRxDisTx";
    case 0x02:
        return "DisRxEnableTx";
    case 0x03:
        return "DisableRxTx (Silent)";
    default:
        return "Unknown";
    }
}

/**
 * @brief  Dump all registered services and Server State to console.
 * @param  env Pointer to the UDS environment handle.
 */
void rtt_uds_dump_services(rtt_uds_env_t *env)
{
    if (!env)
    {
        rt_kprintf("UDS Environment is NULL.\n");
        return;
    }

    UDSServer_t *srv = &env->server;

    rt_kprintf("\n");
    rt_kprintf("============================== UDS Server Status ===============================\n");
    rt_kprintf(" [State]\n");
    rt_kprintf("  Session Type   : %s\n", get_session_name(srv->sessionType));
    rt_kprintf("  Security Level : 0x%02X (%s)\n", srv->securityLevel, srv->securityLevel == 0 ? "Locked" : "Unlocked");
    rt_kprintf("  P2 Timing      : P2=%dms, P2*=%ldms\n", srv->p2_ms, srv->p2_star_ms);
    rt_kprintf("  CommCtrl (Norm): 0x%02X - %s\n", srv->commState_Normal, get_comm_ctrl_name(srv->commState_Normal));
    rt_kprintf("  CommCtrl (NM)  : 0x%02X - %s\n", srv->commState_NM, get_comm_ctrl_name(srv->commState_NM));

    rt_kprintf("\n [Registered Handlers]\n");
    rt_kprintf("%-30s | %-35s | %-4s | %s\n",
               "Node Name", "Event ID", "Prio", "Handler Addr");
    rt_kprintf("-------------------------------+-------------------------------------+------+------------\n");

    int count = 0;

    for (int i = 0; i < UDS_RTT_EVENT_TABLE_SIZE; i++)
    {
        rt_list_t *head = &env->event_table[i];
        uds_service_node_t *node;

        if (rt_list_isempty(head))
            continue;

        rt_list_for_each_entry(node, head, list)
        {
            const char *evt_str = UDSEventToStr(node->event);

            rt_kprintf("%-30s | 0x%02X %-30s | %-4d | %p\n",
                       node->name ? node->name : "N/A",
                       node->event,
                       evt_str,
                       node->priority,
                       node->handler);
            count++;
        }
    }

    rt_kprintf("------------------------------------------------------------------------------------\n");
    rt_kprintf("Total Handlers: %d\n", count);
    rt_kprintf("====================================================================================\n");
}

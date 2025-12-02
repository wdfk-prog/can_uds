/**
 * @file iso14229_rtt.h
 * @brief RT-Thread porting layer header for ISO14229 (UDS) library.
 * @details This header defines the configuration structures, service registration mechanism,
 *          priority levels, and public APIs for running the UDS stack within an RT-Thread environment.
 *          It provides an abstraction over threads, message queues, and CAN hardware.
 * 
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
#ifndef __ISO14229_RTT_H__
#define __ISO14229_RTT_H__

#include "iso14229.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Macros & Constants
 * ========================================================================== */

/**
 * @brief  Macro to define a UDS service handler function signature.
 * @details Use this macro to implement callback functions. It abstracts the underlying arguments.
 * 
 * @param  name The name of the function.
 * 
 * @note   Available arguments inside the function:
 *         - UDSServer_t *srv : The UDS server core instance.
 *         - void *data       : Event-specific arguments (e.g., UDSWDBIArgs_t*).
 *         - void *context    : User-defined context pointer registered with the node.
 * 
 * @example
 *         static UDS_HANDLER(handle_led_control) {
 *             UDSWDBIArgs_t *args = (UDSWDBIArgs_t *)data;
 *             // ... logic
 *             return UDS_PositiveResponse;
 *         }
 */
#define UDS_HANDLER(name) \
    UDSErr_t name(UDSServer_t *srv, void *data, void *context)

/**
 * @brief  Custom Status Code: "Success, but Continue Chain".
 * @details Used by handlers (like loggers or observers) that want to process an event
 *          but allow subsequent handlers (with lower priority) to also process it.
 *          Value is -2 (UDS_FAIL is -1, Positive values are NRCs).
 */
#define RTT_UDS_CONTINUE  ((UDSErr_t)-2)

/**
 * @brief  UDS Service Handler Priority Levels.
 * @details Lower numerical value means higher priority (executes earlier).
 *          Range: 0-255.
 */
typedef enum {
    RTT_UDS_PRIO_HIGHEST = 0,    /**< Highest: Security checks, critical intercepts */
    RTT_UDS_PRIO_HIGH    = 64,   /**< High: Core system functions */
    RTT_UDS_PRIO_NORMAL  = 128,  /**< Normal: Standard application logic (default) */
    RTT_UDS_PRIO_LOW     = 192,  /**< Low: Background tasks */
    RTT_UDS_PRIO_LOWEST  = 255   /**< Lowest: Logging, fallback handlers */
} uds_prio_t;

/**
 * @brief Function pointer type for UDS service handler callbacks.
 */
typedef UDSErr_t (*uds_service_handler_t)(UDSServer_t *srv, void *data, void *context);

/* ==========================================================================
 * Type Definitions
 * ========================================================================== */

/**
 * @brief  UDS Service Node Structure.
 * @details Represents a handler for a specific UDS service event.
 *          Instances are linked into the environment's internal dispatch table.
 *          Users should define these statically and register them.
 */
typedef struct
{
    rt_list_t list;                 /**< Internal list node for the dispatch table */
    UDSEvent_t event;               /**< The UDS event ID to handle (e.g., UDS_EVT_WriteDataByIdent) */
    uint8_t priority;               /**< Execution priority (see uds_prio_t) */
    const char *name;               /**< Debug name of the service node */
    uds_service_handler_t handler;  /**< Callback function */
    void *context;                  /**< User context pointer (optional) */
} uds_service_node_t;

/**
 * @brief  Configuration structure for creating a UDS environment.
 * @details Contains initialization parameters for ISO-TP, UDS Server, and RT-Thread resources.
 */
typedef struct
{
    const char *can_name;       /**< Name of the RT-Thread CAN device (e.g., "can1") */
    uint32_t phys_id;           /**< Physical Request CAN ID (Rx) (e.g., 0x7E0) */
    uint32_t func_id;           /**< Functional Request CAN ID (Rx) (e.g., 0x7DF) */
    uint32_t resp_id;           /**< Response CAN ID (Tx) (e.g., 0x7E8) */
    uint32_t func_resp_id;      /**< Functional Response ID (Tx) (usually UDS_TP_NOOP_ADDR) */

    const char *thread_name;    /**< Name of the internal processing thread */
    uint32_t stack_size;        /**< Thread stack size in bytes */
    uint8_t priority;           /**< Thread priority (RT-Thread priority levels) */
    uint32_t rx_mq_pool_size;   /**< Size of RX message queue (max buffered frames) */
} rtt_uds_config_t;

/**
 * @brief Opaque handle for a UDS environment instance.
 * @details Definition hidden in implementation file.
 */
typedef struct rtt_uds_env rtt_uds_env_t;

/* ==========================================================================
 * Service Definition Macros
 * ========================================================================== */

/**
 * @brief  Runtime initialization macro for a service node.
 * @details Use this inside initialization functions if dynamic allocation is needed.
 * 
 * @param _node_ptr Pointer to uds_service_node_t.
 * @param _name_str String name for debugging.
 * @param _event    UDS Event ID.
 * @param _handler  Callback function.
 * @param _ctx      User context pointer.
 * @param _prio     Priority level.
 */
#define RTT_UDS_SERVICE_NODE_INIT(_node_ptr, _name_str, _event, _handler, _ctx, _prio) \
    do { \
        rt_list_init(&(_node_ptr)->list); \
        (_node_ptr)->name = _name_str; \
        (_node_ptr)->event = _event; \
        (_node_ptr)->handler = _handler; \
        (_node_ptr)->context = _ctx; \
        (_node_ptr)->priority = (uint8_t)_prio; \
    } while(0)

/**
 * @brief  Static definition macro for a service node.
 * @details Defines and initializes a static `uds_service_node_t` variable.
 */
#define RTT_UDS_SERVICE_DEFINE(_name, _event, _handler, _ctx, _prio)    \
    static uds_service_node_t _name = {                                 \
        .list = RT_LIST_OBJECT_INIT(_name.list),                        \
        .name = #_name,                                                 \
        .event = _event,                                                \
        .handler = _handler,                                            \
        .context = _ctx,                                                \
        .priority = (uint8_t)_prio                                      \
    }

/**
 * @brief  Declaration macro for service registration functions (Header File).
 * @param  _name Node name (e.g., led_node).
 */
#define RTT_UDS_SERVICE_DECLARE(_name) \
    rt_err_t _name##_register(rtt_uds_env_t *env); \
    void _name##_unregister(void)

/**
 * @brief  Implementation macro for service registration functions (Source File).
 * @details Automatically generates the `_register` and `_unregister` wrapper functions.
 *          Supports custom priority and context.
 * 
 * @param _name    Node name variable (must be defined via RTT_UDS_SERVICE_DEFINE or similar).
 * @param _event   UDS Event ID.
 * @param _handler Callback function.
 * @param _ctx     User context pointer.
 * @param _prio    Priority level.
 */
#define RTT_UDS_SERVICE_DEFINE_OPS_PRO(_name, _event, _handler, _ctx, _prio)    \
    RTT_UDS_SERVICE_DEFINE(_name, _event, _handler, _ctx, _prio);               \
    rt_err_t _name##_register(rtt_uds_env_t *env) {                             \
        return rtt_uds_service_register(env, &_name);                           \
    }                                                                           \
    void _name##_unregister(void) {                                             \
        rtt_uds_service_unregister(&_name);                                     \
    }

/**
 * @brief  Simplified implementation macro (Source File).
 * @details Wrapper for `RTT_UDS_SERVICE_DEFINE_OPS_PRO` using `NULL` context 
 *          and `RTT_UDS_PRIO_NORMAL` priority.
 */
#define RTT_UDS_SERVICE_DEFINE_OPS(_name, _event, _handler) \
    RTT_UDS_SERVICE_DEFINE_OPS_PRO(_name, _event, _handler, NULL, RTT_UDS_PRIO_NORMAL)

/* ==========================================================================
 * Public APIs
 * ========================================================================== */

/**
 * @brief  Create and initialize a UDS service instance.
 * @details Allocates memory, initializes ISO-TP, creates MQ/Thread, and starts the thread.
 * 
 * @param  cfg Pointer to configuration structure.
 * @return Pointer to new instance handle, or RT_NULL on failure.
 */
rtt_uds_env_t *rtt_uds_create(const rtt_uds_config_t *cfg);

/**
 * @brief  Destroy a UDS service instance.
 * @details Stops thread, deletes resources, and frees memory.
 *          Safe to call on partially initialized instances.
 * 
 * @param  env Pointer to the UDS environment handle.
 */
void rtt_uds_destroy(rtt_uds_env_t *env);

/**
 * @brief  Register a service handler node.
 * @details Adds the node to the dispatch table. Supports multiple handlers per event.
 * 
 * @param  env  Pointer to UDS environment.
 * @param  node Pointer to service node structure.
 * @return RT_EOK on success, -RT_EINVAL (invalid arg), -RT_EBUSY (already registered).
 */
rt_err_t rtt_uds_service_register(rtt_uds_env_t *env, uds_service_node_t *node);

/**
 * @brief  Unregister a service handler node.
 * @param  node Pointer to service node structure.
 */
void rtt_uds_service_unregister(uds_service_node_t *node);

/**
 * @brief  Unregister ALL service handlers.
 * @details Helper to clear the entire dispatch table.
 * @param  env Pointer to UDS environment.
 */
void rtt_uds_service_unregister_all(rtt_uds_env_t *env);

/**
 * @brief  Feed a CAN frame into the UDS stack.
 * @details Non-blocking call. Safe for ISRs. Puts frame into internal MQ.
 * 
 * @param  env Pointer to UDS environment.
 * @param  msg Pointer to received RT-Thread CAN message.
 * @return RT_EOK on success, -RT_EFULL if queue full.
 */
rt_err_t rtt_uds_feed_can_frame(rtt_uds_env_t *env, struct rt_can_msg *msg);

/* ==========================================================================
 * Debug & Utility APIs
 * ========================================================================== */

/**
 * @brief  Log hex data to console.
 * @param  title Description string.
 * @param  data  Data buffer.
 * @param  size  Size in bytes.
 */
void rtt_uds_log_hex(const char *title, const uint8_t *data, rt_size_t size);

/**
 * @brief  Dump all registered services and server state to console.
 * @details Useful for debugging configuration.
 * @param  env Pointer to UDS environment.
 */
void rtt_uds_dump_services(rtt_uds_env_t *env);

/**
 * @brief  Check if Application TX is allowed.
 * @details Based on Service 0x28 (Communication Control) state for Normal messages.
 * @param  env Pointer to UDS environment.
 * @return 1 if allowed, 0 if disabled.
 */
int rtt_uds_is_app_tx_enabled(rtt_uds_env_t *env);

/**
 * @brief  Check if Application RX is allowed.
 * @details Based on Service 0x28 (Communication Control) state for Normal messages.
 * @param  env Pointer to UDS environment.
 * @return 1 if allowed, 0 if disabled (drop message).
 */
int rtt_uds_is_app_rx_enabled(rtt_uds_env_t *env);

/* ==========================================================================
 * Special Registration Helpers
 * ========================================================================== */

/**
 * @brief  Register the built-in Session Timeout logger.
 * @param  env Pointer to UDS environment.
 */
void log_timeout_node_register(rtt_uds_env_t *env);

#ifdef __cplusplus
}
#endif

#endif /* __ISO14229_RTT_H__ */
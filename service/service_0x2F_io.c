/**
 * @file service_0x2F_io.c
 * @brief Implementation of generic InputOutputControlByIdentifier (0x2F) service.
 * @details Handles UDS IO requests and dispatches them to specific DID handlers.
 *          Manages the "ReturnControlToECU" logic upon session timeout.
 *          Supports multiple independent IO service instances.
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2025-11-29
 * 
 * @copyright Copyright (c) 2025  
 * 
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2025-11-29 1.0     wdfk-prog   first version
 */
#include "rtt_uds_service.h"

#define DBG_TAG "uds.io"
#define DBG_LVL DBG_WARNING
#include <rtdbg.h>

#ifdef UDS_ENABLE_0X2F_IO_SVC

/* ==========================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief  Find an IO Node by its Data Identifier (DID).
 * @param  svc IO Service context.
 * @param  did The DID to search for.
 * @return Pointer to the node if found, otherwise RT_NULL.
 */
static uds_io_node_t *find_node_by_did(const uds_io_service_t *svc, uint16_t did)
{
    uds_io_node_t *node;
    rt_list_for_each_entry(node, &svc->node_list, list)
    {
        if (node->did == did)
        {
            return node;
        }
    }
    return RT_NULL;
}

/* ==========================================================================
 * UDS Service Handlers
 * ========================================================================== */

/**
 * @brief  Dispatcher for IO Control (0x2F) Requests.
 * @details Routes the request to the registered handler for the specific DID.
 *          Updates the override status based on the action.
 */
static UDS_HANDLER(handle_io_control_dispatch)
{
    uds_io_service_t *svc = (uds_io_service_t *)context;
    if (!svc)
        return UDS_NRC_ConditionsNotCorrect;

    UDSIOCtrlArgs_t *args = (UDSIOCtrlArgs_t *)data;

    /* 1. Prepare Response Buffer */
    uint8_t resp_buf[UDS_IO_MAX_RESP_LEN];
    size_t resp_len = UDS_IO_MAX_RESP_LEN;

    /* 2. Find Node */
    uds_io_node_t *node = find_node_by_did(svc, args->dataId);
    if (!node)
        return UDS_NRC_RequestOutOfRange;

    LOG_I("IO Req DID:0x%04X Action:0x%02X", args->dataId, args->ioCtrlParam);

    uds_io_action_t action = (uds_io_action_t)args->ioCtrlParam;

    /* 3. Execute User Callback */
    UDSErr_t res = node->handler(node->did,
                                 action,
                                 args->ctrlStateAndMask,
                                 args->ctrlStateAndMaskLen,
                                 resp_buf,
                                 &resp_len);

    if (res != UDS_PositiveResponse)
    {
        return res;
    }

    /* 4. Update Override State */
    if (action == UDS_IO_ACT_SHORT_TERM_ADJ || action == UDS_IO_ACT_FREEZE_CURRENT)
    {
        node->is_overridden = 1;
    }
    else if (action == UDS_IO_ACT_RETURN_CONTROL || action == UDS_IO_ACT_RESET_TO_DEFAULT)
    {
        node->is_overridden = 0;
    }

    /* 5. Send Response */
    if (resp_len > UDS_IO_MAX_RESP_LEN)
        resp_len = UDS_IO_MAX_RESP_LEN;

    return args->copy(srv, resp_buf, (uint16_t)resp_len);
}

/**
 * @brief  Handler for Session Timeout Events.
 * @details Automatically releases control of all overridden DIDs when the 
 *          diagnostic session reverts to Default.
 */
static UDS_HANDLER(handle_io_session_timeout)
{
    uds_io_service_t *svc = (uds_io_service_t *)context;
    if (!svc)
        return RTT_UDS_CONTINUE;

    uds_io_node_t *node;
    uint8_t dummy_buf[UDS_IO_MAX_RESP_LEN];
    size_t dummy_len;
    UDSErr_t err;

    /* Iterate all nodes to check override status */
    rt_list_for_each_entry(node, &svc->node_list, list)
    {
        if (node->is_overridden)
        {
            LOG_W("Timeout: Auto-releasing DID 0x%04X", node->did);

            /* Reset length variable for each iteration */
            dummy_len = UDS_IO_MAX_RESP_LEN;

            /* Execute ReturnControl callback */
            err = node->handler(node->did,
                                UDS_IO_ACT_RETURN_CONTROL,
                                NULL, 0,
                                dummy_buf,
                                &dummy_len);

            /* Even if callback fails, we must clear the flag to stay consistent with session state */
            if (err != UDS_PositiveResponse)
            {
                LOG_E("Failed to release DID 0x%04X (Err: 0x%02X)", node->did, err);
            }

            node->is_overridden = 0;
        }
    }

    return RTT_UDS_CONTINUE; /* Continue chain for other handlers */
}

/* ==========================================================================
 * Public API Implementation
 * ========================================================================== */
/**
 * @brief  Check if a specific DID is currently controlled by UDS.
 * @details Helper for application logic to decide whether to update hardware.
 * @param  svc Pointer to IO service context.
 * @param  did The DID to check.
 * @return Status code:
 *         1 : Overridden (UDS has control).
 *         0 : Free (Application has control) or svc is NULL.
 *        -1 : DID not found in the service list.
 */
int uds_io_is_did_overridden(uds_io_service_t *svc, uint16_t did)
{
    if (!svc)
        return 0;

    uds_io_node_t *node = find_node_by_did(svc, did);

    if (node)
    {
        return node->is_overridden;
    }
    return -1; /* DID not found */
}

/**
 * @brief  Register a hardware node to the IO Service.
 * @param  svc  Pointer to IO service context.
 * @param  node Pointer to IO node.
 * @return RT_EOK on success.
 *         -RT_EINVAL if arguments are NULL or handler is missing.
 *         -RT_EBUSY if the node is already registered (linked to a list).
 */
rt_err_t uds_io_register_node(uds_io_service_t *svc, uds_io_node_t *node)
{
    if (!svc || !node || !node->handler)
        return -RT_EINVAL;

    /* Check if already registered to avoid list corruption */
    if (!rt_list_isempty(&node->list))
    {
        LOG_W("Node DID 0x%04X already registered!", node->did);
        return -RT_EBUSY;
    }

    node->is_overridden = 0;
    rt_list_insert_before(&svc->node_list, &node->list);
    LOG_D("IO Node Registered: DID 0x%04X", node->did);
    return RT_EOK;
}

/**
 * @brief  Unregister a hardware node from the IO Service.
 * @note   Does NOT automatically reset hardware state if currently overridden.
 * @param  svc  Pointer to IO service context.
 * @param  node Pointer to IO node.
 */
void uds_io_unregister_node(uds_io_service_t *svc, uds_io_node_t *node)
{
    if (!svc || !node)
        return;

    /* Check if actually in a list */
    if (rt_list_isempty(&node->list))
        return;

    rt_list_remove(&node->list);

    /* Mark as detached */
    rt_list_init(&node->list);

    LOG_D("IO Node Unregistered: DID 0x%04X", node->did);
}

/**
 * @brief  Mount the IO Service to the UDS Core Environment.
 * @details Registers the internal 0x2F and Timeout handlers to the UDS dispatcher.
 * @param  env Pointer to UDS environment.
 * @param  svc Pointer to IO service context.
 * @return RT_EOK on success.
 */
rt_err_t rtt_uds_io_service_mount(rtt_uds_env_t *env, uds_io_service_t *svc)
{
    if (!env || !svc)
        return -RT_EINVAL;

    /* 
     * Determine names.
     * If user defined via RTT_UDS_IO_SERVICE_DEFINE, names are pre-filled.
     * If dynamically allocated, names might be NULL, use defaults.
     */
    const char *ctrl_name = svc->ctrl_service_node.name ? svc->ctrl_service_node.name : "io_ctrl";
    const char *tmout_name = svc->timeout_service_node.name ? svc->timeout_service_node.name : "io_timeout";

    /* Initialize internal service nodes */
    RTT_UDS_SERVICE_NODE_INIT(&svc->ctrl_service_node,
                              ctrl_name,
                              UDS_EVT_IOControl,
                              handle_io_control_dispatch,
                              svc,
                              RTT_UDS_PRIO_NORMAL);

    RTT_UDS_SERVICE_NODE_INIT(&svc->timeout_service_node,
                              tmout_name,
                              UDS_EVT_SessionTimeout,
                              handle_io_session_timeout,
                              svc,
                              RTT_UDS_PRIO_HIGH);

    /* Register to Core */
    rt_err_t ret;
    ret = rtt_uds_service_register(env, &svc->ctrl_service_node);
    if (ret != RT_EOK)
        return ret;

    ret = rtt_uds_service_register(env, &svc->timeout_service_node);
    return ret;
}

/**
 * @brief  Unmount the IO Service from the UDS Core.
 * @param  svc Pointer to IO service context.
 */
void rtt_uds_io_service_unmount(uds_io_service_t *svc)
{
    if (!svc)
        return;

    rtt_uds_service_unregister(&svc->ctrl_service_node);
    rtt_uds_service_unregister(&svc->timeout_service_node);

    LOG_I("IO Service Unmounted");
}

#endif /* UDS_ENABLE_0X2F_IO_SVC */

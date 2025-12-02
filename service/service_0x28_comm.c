/**
 * @file service_0x28_comm.c
 * @brief Implementation of UDS Service 0x28 (Communication Control).
 * @details Handles requests to enable/disable transmission and reception of 
 *          specific message groups (Normal/NM). 
 *          Supports both global control (SubFunction 0x00-0x03) and 
 *          node-specific control with enhanced addressing (0x04-0x05).
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

#define DBG_TAG "uds.cc"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef UDS_ENABLE_0X28_COMM_CTRL_SVC

/* ==========================================================================
 * Internal Helper Functions
 * ========================================================================== */

/**
 * @brief  Apply Communication State to the Server Context.
 * @details Updates the internal state variables (`commState_Normal`, `commState_NM`)
 *          based on the requested communication scope.
 * 
 * @param  srv        Pointer to the UDS server instance.
 * @param  ctrl_type  The target control state (e.g., EnableRxAndTx).
 * @param  comm_type  The scope (Normal, NM, or Both).
 */
static void apply_comm_state(UDSServer_t *srv, uint8_t ctrl_type, uint8_t comm_type)
{
    uint8_t scope = comm_type & 0x03;

    if (scope == UDS_CTP_NCM)             /* 1: Normal Communication Messages */
    {
        srv->commState_Normal = ctrl_type;
    } 
    else if (scope == UDS_CTP_NWMCM)      /* 2: Network Management Messages */
    {
        srv->commState_NM = ctrl_type;
    } 
    else if (scope == UDS_CTP_NWMCM_NCM)  /* 3: Both */
    {
        srv->commState_Normal = ctrl_type;
        srv->commState_NM     = ctrl_type;
    }
    
    LOG_I("CC State Updated: Norm=%d, NM=%d", srv->commState_Normal, srv->commState_NM);
}

/* ==========================================================================
 * UDS Service Handlers
 * ========================================================================== */

/**
 * @brief  Handler for Service 0x28 (CommunicationControl).
 * @details 
 *  - For Global Control types (0x00-0x03): The ISO14229 core library automatically 
 *    updates the state *after* this handler returns PositiveResponse.
 *  - For Node-Specific types (0x04-0x05): The core library CANNOT update the state 
 *    automatically because it doesn't know the local Node ID. This handler MUST 
 *    check the Node ID and update the state manually if matched.
 */
static UDS_HANDLER(handle_comm_control)
{
    uds_comm_ctrl_service_t *svc = (uds_comm_ctrl_service_t *)context;
    if (!svc) return UDS_NRC_ConditionsNotCorrect;

    UDSCommCtrlArgs_t *args = (UDSCommCtrlArgs_t *)data;
    uint8_t ctrl = args->ctrlType;
    uint8_t comm = args->commType;
    uint16_t req_id = args->nodeId;

    /* 
     * Case 1: Global Control (0x00 ~ 0x03)
     * The core library handles the state update. We just log and approve.
     */
    if (ctrl <= UDS_LEV_CTRLTP_DRXTX) 
    {
        LOG_I("CC Global Req: Ctrl=0x%02X Comm=0x%02X", ctrl, comm);
        return UDS_PositiveResponse;
    }

    /* 
     * Case 2: Node-Specific Control (0x04, 0x05)
     * We must check the NodeID and manually update state if matched.
     */
    uint8_t target_mode = 0xFF;

    if (ctrl == UDS_LEV_CTRLTP_ERXDTXWEAI) /* 0x04: EnableRxDisableTx (addressed) */
    {
        if (req_id == svc->node_id) 
        {
            /* Map to equivalent global state: EnableRxAndDisableTx (0x01) */
            target_mode = UDS_LEV_CTRLTP_ERXDTX; 
            LOG_I("CC Match (0x%04X): Disabling TX", req_id);
        }
        else
        {
            LOG_D("CC Ignore (0x%04X != 0x%04X)", req_id, svc->node_id);
        }
    }
    else if (ctrl == UDS_LEV_CTRLTP_ERXTXWEAI) /* 0x05: EnableRxAndTx (addressed) */
    {
        if (req_id == svc->node_id) 
        {
            /* Map to equivalent global state: EnableRxAndTx (0x00) */
            target_mode = UDS_LEV_CTRLTP_ERXTX;
            LOG_I("CC Match (0x%04X): Enabling All", req_id);
        }
        else
        {
            LOG_D("CC Ignore (0x%04X != 0x%04X)", req_id, svc->node_id);
        }
    }
    else 
    {
        /* Unknown/Unsupported SubFunction */
        return UDS_NRC_RequestOutOfRange;
    }

    /* 3. Manually update core state if ID matched */
    if (target_mode != 0xFF) 
    {
        apply_comm_state(srv, target_mode, comm);
    }

    return UDS_PositiveResponse;
}

/* ==========================================================================
 * Public API Implementation
 * ========================================================================== */

/**
 * @brief  Update the Node ID at runtime.
 * @param  svc     Pointer to service context.
 * @param  node_id New Node ID.
 */
void rtt_uds_comm_ctrl_set_id(uds_comm_ctrl_service_t *svc, uint16_t node_id)
{
    if (svc) 
    {
        svc->node_id = node_id;
        LOG_D("CC: Node ID set to 0x%04X", node_id);
    }
}

/**
 * @brief  Mount the 0x28 Service to the UDS Core.
 * @param  env Pointer to UDS environment.
 * @param  svc Pointer to service context.
 * @return RT_EOK on success.
 */
rt_err_t rtt_uds_comm_ctrl_service_mount(rtt_uds_env_t *env, uds_comm_ctrl_service_t *svc)
{
    if (!env || !svc) return -RT_EINVAL;

    /* 
     * Bind Handler and Event to the node.
     * This ensures the node is fully configured even if initialized dynamically.
     */
    svc->ctrl_service_node.event = UDS_EVT_CommCtrl;
    svc->ctrl_service_node.handler = handle_comm_control; /* Bind static handler */
    svc->ctrl_service_node.priority = RTT_UDS_PRIO_NORMAL;
    svc->ctrl_service_node.context = svc; /* Ensure context points to service struct */
    
    /* Default name protection */
    if (svc->ctrl_service_node.name == RT_NULL) 
    {
        svc->ctrl_service_node.name = "cc_ctrl";
    }

    return rtt_uds_service_register(env, &svc->ctrl_service_node);
}

/**
 * @brief  Unmount the 0x28 Service from the UDS Core.
 * @param  svc Pointer to service context.
 */
void rtt_uds_comm_ctrl_service_unmount(uds_comm_ctrl_service_t *svc)
{
    if (!svc) return;
    
    rtt_uds_service_unregister(&svc->ctrl_service_node);
    
    LOG_I("CC Service Unmounted");
}

#endif /* UDS_ENABLE_0X28_COMM_CTRL_SVC */
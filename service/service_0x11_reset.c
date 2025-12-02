/**
 * @file service_0x11_reset.c
 * @brief Implementation of UDS Service 0x11 (ECUReset).
 * @details Handles the two-stage reset process:
 *          1. Validating the request and scheduling the reset (UDS_EVT_EcuReset).
 *          2. Executing the physical reset after the response is sent (UDS_EVT_DoScheduledReset).
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
#include <rthw.h> /* For rt_hw_cpu_reset() */

#define DBG_TAG "uds.reset"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef UDS_ENABLE_0X11_RESET_SVC

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/**
 * @brief Reset Delay in milliseconds.
 * @details The time to wait between sending the Positive Response and performing 
 *          the physical CPU reset. This ensures the CAN frame is fully transmitted 
 *          from the hardware mailbox.
 */
#ifndef UDS_RESET_DELAY_MS
#define UDS_RESET_DELAY_MS  50
#endif

#define RESET_DELAY_MS  UDS_RESET_DELAY_MS

/* ==========================================================================
 * Service Handlers
 * ========================================================================== */

/**
 * @brief  Stage 1: Handle ECU Reset Request (0x11).
 * @details Validates the reset type and security level. If accepted, it sets the 
 *          `powerDownTimeMillis` to schedule the actual reset event.
 * 
 * @param  srv     UDS Server instance.
 * @param  data    Pointer to UDSECUResetArgs_t.
 * @param  context Unused.
 * @return UDS_PositiveResponse if accepted, NRC otherwise.
 */
static UDS_HANDLER(handle_ecu_reset_request)
{
    UDSECUResetArgs_t *args = (UDSECUResetArgs_t *)data;

    LOG_I("ECU Reset Request: Type 0x%02X", args->type);

    switch (args->type)
    {
    case UDS_LEV_RT_HR:      /* 0x01: Hard Reset (Simulate power cycle) */
    case UDS_LEV_RT_KOFFONR: /* 0x02: Key Off On Reset */
    case UDS_LEV_RT_SR:      /* 0x03: Soft Reset */

        /* 
         * Critical Logic: Set powerDownTimeMillis.
         * The core library will send the Positive Response immediately,
         * then wait for this duration before triggering UDS_EVT_DoScheduledReset.
         */
        args->powerDownTimeMillis = RESET_DELAY_MS;

        LOG_I("Reset Accepted. Scheduling reset in %d ms...", RESET_DELAY_MS);
        return UDS_PositiveResponse;

    case UDS_LEV_RT_ERPSD:   /* 0x04: Enable Rapid Power Shutdown */
    case UDS_LEV_RT_DRPSD:   /* 0x05: Disable Rapid Power Shutdown */
        /* Return SFNS if power management is not supported by hardware */
        return UDS_NRC_SubFunctionNotSupported;

    default:
        return UDS_NRC_SubFunctionNotSupported;
    }
}

/**
 * @brief  Stage 2: Perform Physical Reset.
 * @details Triggered by the UDS library after the positive response has been sent
 *          and the `powerDownTimeMillis` has elapsed.
 * 
 * @param  srv     UDS Server instance.
 * @param  data    Pointer to uint8_t (reset_type).
 * @param  context Unused.
 * @return Does not return (System Resets).
 */
static UDS_HANDLER(handle_perform_reset)
{
    uint8_t reset_type = *(uint8_t *)data;

    LOG_W("!!! SYSTEM RESET NOW (Type: 0x%02X) !!!", reset_type);

    /* 
     * Wait briefly to ensure the log message is flushed to UART/Console.
     * This depends on the baud rate and buffering strategy.
     */
    rt_thread_mdelay(RESET_DELAY_MS);

    /* RT-Thread Standard Interface: CPU Reset */
    rt_hw_cpu_reset();

    /* Code should not reach here */
    while (1);

    return UDS_PositiveResponse;
}

/* ==========================================================================
 * Service Registration
 * ========================================================================== */

/* 
 * Define static service nodes.
 * This generates the following functions:
 * - reset_req_node_register(env) / reset_req_node_unregister()
 * - reset_exec_node_register(env) / reset_exec_node_unregister()
 */
RTT_UDS_SERVICE_DEFINE_OPS(reset_req_node, UDS_EVT_EcuReset, handle_ecu_reset_request);
RTT_UDS_SERVICE_DEFINE_OPS(reset_exec_node, UDS_EVT_DoScheduledReset, handle_perform_reset);

#endif /* UDS_ENABLE_0X11_RESET_SVC */

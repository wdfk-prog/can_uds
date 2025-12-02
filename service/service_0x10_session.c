/**
 * @file service_0x10_session.c
 * @brief Implementation of Service 0x10 (Diagnostic Session Control).
 * @details Handles session transitions (Default, Programming, Extended) and 
 *          negotiates P2/P2* timing parameters with the client.
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

#define DBG_TAG "uds.session"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/* 
 * P2 Server Max: Max time from Request RX to Response TX.
 * P2* Server Max: Max time from RCRRP (0x78) to next response.
 */

/** 
 * @brief Standard Timing (Default Session) 
 * @note  P2=50ms, P2*=2000ms (ISO 14229-2 standard values)
 */
#ifndef UDS_P2_MS_STD
#define UDS_P2_MS_STD           50
#endif

#ifndef UDS_P2_STAR_MS_STD
#define UDS_P2_STAR_MS_STD      2000
#endif

/** 
 * @brief Extended Timing (Extended/Programming Session)
 * @note  P2=5000ms allows for heavy ISO-TP transfers (e.g., File transfer, Console output)
 *        without timing out.
 */
#ifndef UDS_P2_MS_EXT
#define UDS_P2_MS_EXT           5000 
#endif

#ifndef UDS_P2_STAR_MS_EXT
#define UDS_P2_STAR_MS_EXT      5000
#endif

/* Local mapping to keep logic code clean */
#define P2_MS_STD           UDS_P2_MS_STD
#define P2_STAR_MS_STD      UDS_P2_STAR_MS_STD
#define P2_MS_EXT           UDS_P2_MS_EXT
#define P2_STAR_MS_EXT      UDS_P2_STAR_MS_EXT

/* ==========================================================================
 * Service 0x10 Handlers
 * ========================================================================== */

/**
 * @brief  Handler for Service 0x10 (Diagnostic Session Control).
 * @details Processes the session switch request and updates timing parameters 
 *          in the argument structure, which the core library uses to update its state.
 * 
 * @param  srv     UDS Server instance.
 * @param  data    Pointer to UDSDiagSessCtrlArgs_t.
 * @param  context Unused.
 * @return UDS_PositiveResponse or NRC.
 */
static UDS_HANDLER(handle_session_control)
{
    UDSDiagSessCtrlArgs_t *args = (UDSDiagSessCtrlArgs_t *)data;

    LOG_I("Request Session Type: 0x%02X", args->type);

    switch (args->type)
    {
    case UDS_LEV_DS_DS: /* Default Session (0x01) */
        /* Restore standard timings */
        args->p2_ms = P2_MS_STD;
        args->p2_star_ms = P2_STAR_MS_STD;
        LOG_I("Switch to Default Session (Std Timing)");
        return UDS_PositiveResponse;

    case UDS_LEV_DS_PRGS: /* Programming Session (0x02) */
        /* 
         * Note: In a full bootloader, this might trigger a reboot check.
         * Here we just enable the session and relax timings.
         */
        args->p2_ms = P2_MS_EXT;
        args->p2_star_ms = P2_STAR_MS_EXT;
        LOG_I("Switch to Programming Session (Ext Timing)");
        return UDS_PositiveResponse;

    case UDS_LEV_DS_EXTDS: /* Extended Diagnostic Session (0x03) */
        /* Enable high-throughput modes (File Transfer, Console, etc.) */
        args->p2_ms = P2_MS_EXT;
        args->p2_star_ms = P2_STAR_MS_EXT;
        LOG_I("Switch to Extended Session (Ext Timing)");
        return UDS_PositiveResponse;

    default:
        /* 0x7E: SubFunction Not Supported In Active Session (or just Not Supported) */
        LOG_W("Invalid Session Type: 0x%02X", args->type);
        return UDS_NRC_SubFunctionNotSupported;
    }
}

/* ==========================================================================
 * Service Registration
 * ========================================================================== */

/* 
 * Defines 'session_control_node_register' and 'session_control_node_unregister'.
 * Binds the 'UDS_EVT_DiagSessCtrl' event to the handler.
 */
RTT_UDS_SERVICE_DEFINE_OPS(session_control_node, UDS_EVT_DiagSessCtrl, handle_session_control);

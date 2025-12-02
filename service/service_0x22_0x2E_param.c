/**
 * @file service_0x22_0x2E_param.c
 * @brief UDS service implementation for Parameter Management (0x22/0x2E).
 * @details - 0x22 Read Data By Identifier (RDBI)
 *          - 0x2E Write Data By Identifier (WDBI)
 * 
 * @author wdfk-prog ()
 * @version 1.0
 * @date 2025-11-29
 * 
 * @copyright Copyright (c) 2025  
 * 
 * @note    IMPORTANT:
 *          This file is an EXAMPLE integration. It depends on external modules:
 *          - parameter_manager.h (parameter_get, parameter_set)
 *          - general.h / general_extend.h (parameter objects)
 *          These functions are NOT implemented in the UDS library. 
 *          You must provide the backend implementation or adapt this file to 
 *          your specific non-volatile memory (NVM) manager.
 * @par Change Log:
 * Date       Version Author      Description
 * 2025-11-29 1.0     wdfk-prog   first version
 */
#include "rtt_uds_service.h"

/* 
 * External Dependencies 
 * (Ensure these headers exist in your project or replace with your own NVM API)
 */
#include "parameter_manager.h"
#include "general.h"
#include "general_extend.h"
#include "common_macro.h"

#define DBG_TAG "uds.param"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef UDS_ENABLE_PARAM_SVC

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/** 
 * @brief Max buffer size for reading a single parameter.
 * @details Ensure this is large enough to hold the largest parameter structure 
 *          defined in your system.
 */
#ifndef UDS_PARAM_RDBI_BUF_SIZE
#define UDS_PARAM_RDBI_BUF_SIZE 64
#endif

#define PARAM_RDBI_BUF_SIZE UDS_PARAM_RDBI_BUF_SIZE

/* ==========================================================================
 * Internal Helper Functions (Backend Wrappers)
 * ========================================================================== */

/**
 * @brief  Wrapper to read a parameter from the legacy manager.
 * @param  obj        Pointer to the parameter object (e.g., general_obj).
 * @param  index      The parameter index (Mapped from UDS DID).
 * @param  data       Buffer to store the read data.
 * @param  len_out    [Output] The length of the data actually read.
 * @param  readlevel  Security access level (legacy argument).
 * @return UDS_PositiveResponse or an appropriate NRC.
 */
static UDSErr_t helper_param_read(struct paragen_object const * const obj,
                                  U32 index,
                                  void *data,
                                  uint16_t *len_out,
                                  U32 readlevel)
{
    U32 data_len = 0;
    RC ret;

    /* 
     * Call external API: parameter_get
     * Note: Ensure 'data' buffer is sufficient. Legacy API often lacks size arg.
     */
    ret = parameter_get(obj, index, data, &data_len, readlevel);

    switch (ret)
    {
    case RC_SUCCESS:
        *len_out = (uint16_t)data_len;
        return UDS_PositiveResponse;

    case RC_ERROR_RANGE:
        /* Index/DID not found in this object table */
        return UDS_NRC_RequestOutOfRange;

    case RC_ERROR_OPEN:
    case RC_ERROR_READ_FAILS:
    case RC_ERROR_INVALID:
        /* Hardware or Logic error */
        return UDS_NRC_ConditionsNotCorrect;

    default:
        return UDS_NRC_GeneralReject;
    }
}

/**
 * @brief  Wrapper to write a parameter to the legacy manager.
 * @param  obj              Pointer to the parameter object.
 * @param  index            The parameter index (Mapped from UDS DID).
 * @param  data             Pointer to the data to write.
 * @param  size             Size of the data.
 * @param  if_write_eeprom  TRUE to save to NVM, FALSE for RAM only.
 * @return UDS_PositiveResponse or an appropriate NRC.
 */
static UDSErr_t helper_param_write(struct paragen_object const * const obj,
                                   U32 index,
                                   void *data,
                                   uint16_t size,
                                   BO if_write_eeprom)
{
    RC ret;

    /* Call external API: parameter_set */
    ret = parameter_set(obj, index, data, (U32)size, if_write_eeprom);

    switch (ret)
    {
    case RC_SUCCESS:
        return UDS_PositiveResponse;

    case RC_ERROR_RANGE:
        return UDS_NRC_RequestOutOfRange;

    case RC_ERROR_FILE_ACCESS:
        return UDS_NRC_SecurityAccessDenied;

    case RC_ERROR_OPEN:
    case RC_ERROR_READ_FAILS:
    case RC_ERROR_INVALID:
        return UDS_NRC_ConditionsNotCorrect;

    case RC_ERROR:
    default:
        return UDS_NRC_GeneralReject;
    }
}

/* ==========================================================================
 * UDS Service Handlers
 * ========================================================================== */

/**
 * @brief  Handler for Service 0x22 (ReadDataByIdentifier).
 * @details Implements a lookup strategy:
 *          1. Try 'general_extend_obj' (Common/Global IDs).
 *          2. If not found, try 'general_obj' (Local/Legacy IDs).
 * 
 * @param  srv     UDS Server instance.
 * @param  data    Pointer to UDSRDBIArgs_t.
 * @param  context Unused.
 * @return UDS_PositiveResponse or NRC.
 */
static UDS_HANDLER(handle_rdbi)
{
    UDSRDBIArgs_t *args = (UDSRDBIArgs_t *)data;
    UDSErr_t result;
    uint16_t read_len = 0;

    /* Temporary buffer for parameter value (Stack allocated) */
    uint8_t temp_buf[PARAM_RDBI_BUF_SIZE];

    /* 1. Attempt read from Extended Object (Common IDs) */
    result = helper_param_read(&general_extend_obj,
                               (U32)args->dataId,
                               temp_buf,
                               &read_len,
                               0); /* ReadLevel 0 */

    /* 2. If failed (Not Found), attempt read from General Object (Local IDs) */
    if (result == UDS_NRC_RequestOutOfRange)
    {
        /* Reset length before second attempt */
        read_len = 0;
        result = helper_param_read(&general_obj,
                                   (U32)args->dataId,
                                   temp_buf,
                                   &read_len,
                                   0);
    }

    if (result == UDS_PositiveResponse)
    {
        /* Check if data fits in the UDS response PDU is handled by args->copy internally */
        return args->copy(srv, temp_buf, read_len);
    }

    /* Return the failure code (likely RequestOutOfRange if neither had it) */
    return result;
}

/**
 * @brief  Handler for Service 0x2E (WriteDataByIdentifier).
 * @details Uses the same lookup strategy as RDBI. 
 *          Writes are persisted to EEPROM (TRUE flag).
 * 
 * @param  srv     UDS Server instance.
 * @param  data    Pointer to UDSWDBIArgs_t.
 * @param  context Unused.
 * @return UDS_PositiveResponse or NRC.
 */
static UDS_HANDLER(handle_wdbi)
{
    UDSWDBIArgs_t *args = (UDSWDBIArgs_t *)data;
    UDSErr_t result;

    /* 1. Attempt write to Extended Object */
    result = helper_param_write(&general_extend_obj,
                                (U32)args->dataId,
                                (void *)args->data,
                                args->len,
                                TRUE); /* Persist to NVM */

    /* 2. If failed (Not Found), attempt write to General Object */
    if (result == UDS_NRC_RequestOutOfRange)
    {
        result = helper_param_write(&general_obj,
                                    (U32)args->dataId,
                                    (void *)args->data,
                                    args->len,
                                    TRUE);
    }

    return result;
}

/* ==========================================================================
 * Service Registration
 * ========================================================================== */

/* 
 * Defines the registration functions:
 * - param_rdbi_node_register / unregister
 * - param_wdbi_node_register / unregister
 */
RTT_UDS_SERVICE_DEFINE_OPS(param_rdbi_node, UDS_EVT_ReadDataByIdent, handle_rdbi);
RTT_UDS_SERVICE_DEFINE_OPS(param_wdbi_node, UDS_EVT_WriteDataByIdent, handle_wdbi);

#endif /* UDS_ENABLE_PARAM_SVC */

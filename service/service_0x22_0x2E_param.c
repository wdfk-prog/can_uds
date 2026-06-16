/**
 * @file service_0x22_0x2E_param.c
 * @brief UDS service implementation for Parameter Management (0x22/0x2E).
 * @details - 0x22 Read Data By Identifier (RDBI)
 *          - 0x2E Write Data By Identifier (WDBI)
 *
 * @author wdfk-prog ()
 * @version 1.1
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2025
 *
 * @note    IMPORTANT:
 *          This file is an EXAMPLE integration with autogen_parameter_manager.
 *          UDS DID values are mapped directly to the generated external
 *          parameter IDs. This example supports scalar parameters only and
 *          transfers values as raw native memory without byte-order conversion.
 * @par Change Log:
 * Date       Version Author      Description
 * 2025-11-29 1.0     wdfk-prog   first version
 * 2026-06-16 1.1     wdfk-prog   use autogen_parameter_manager backend
 */
#include "rtt_uds_service.h"

#include "par.h"
#include <string.h>

#define DBG_TAG "uds.param"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef UDS_ENABLE_PARAM_SVC

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/**
 * @brief Max buffer size for reading a single scalar parameter.
 * @details The scalar-only example currently needs at most 4 bytes. The macro
 *          is kept configurable so the example can be extended without changing
 *          the service code.
 */
#ifndef UDS_PARAM_RDBI_BUF_SIZE
#define UDS_PARAM_RDBI_BUF_SIZE 8
#endif /* !defined(UDS_PARAM_RDBI_BUF_SIZE) */

#define PARAM_RDBI_BUF_SIZE UDS_PARAM_RDBI_BUF_SIZE

/* ==========================================================================
 * Internal Helper Functions (autogen_parameter_manager Backend Wrappers)
 * ========================================================================== */

/**
 * @brief Convert a parameters status code to a UDS negative response code.
 * @param status Operation status returned by the parameters module.
 * @return UDS_PositiveResponse or an appropriate NRC.
 */
static UDSErr_t helper_param_status_to_nrc(par_status_t status)
{
    if ((status & ePAR_STATUS_ERROR_MASK) == 0U)
    {
        return UDS_PositiveResponse;
    }

    if ((status & (ePAR_ERROR_PARAM | ePAR_ERROR_PAR_NUM | ePAR_ERROR_TYPE | ePAR_ERROR_VALUE)) != 0U)
    {
        return UDS_NRC_RequestOutOfRange;
    }

    if ((status & ePAR_ERROR_ACCESS) != 0U)
    {
        return UDS_NRC_SecurityAccessDenied;
    }

    if ((status & (ePAR_ERROR_INIT | ePAR_ERROR_NVM | ePAR_ERROR_MUTEX)) != 0U)
    {
        return UDS_NRC_ConditionsNotCorrect;
    }

    return UDS_NRC_GeneralReject;
}

/**
 * @brief Return the native byte size for one supported scalar parameter type.
 * @param type Parameter scalar type.
 * @param len_out Pointer to the byte-length output.
 * @return UDS_PositiveResponse when the type is supported; otherwise NRC.
 */
static UDSErr_t helper_param_scalar_size(par_type_list_t type, uint16_t *len_out)
{
    switch (type)
    {
    case ePAR_TYPE_U8:
    case ePAR_TYPE_I8:
        *len_out = 1U;
        return UDS_PositiveResponse;

    case ePAR_TYPE_U16:
    case ePAR_TYPE_I16:
        *len_out = 2U;
        return UDS_PositiveResponse;

    case ePAR_TYPE_U32:
    case ePAR_TYPE_I32:
    case ePAR_TYPE_F32:
        *len_out = 4U;
        return UDS_PositiveResponse;

    default:
        return UDS_NRC_RequestOutOfRange;
    }
}

/**
 * @brief Return a pointer to the native scalar union member for one type.
 * @param value Pointer to the scalar value union.
 * @param type Parameter scalar type.
 * @return Pointer to the typed union member, or RT_NULL when unsupported.
 */
static void *helper_param_scalar_ptr(par_type_t *value, par_type_list_t type)
{
    switch (type)
    {
    case ePAR_TYPE_U8:
        return &value->u8;

    case ePAR_TYPE_I8:
        return &value->i8;

    case ePAR_TYPE_U16:
        return &value->u16;

    case ePAR_TYPE_I16:
        return &value->i16;

    case ePAR_TYPE_U32:
        return &value->u32;

    case ePAR_TYPE_I32:
        return &value->i32;

    case ePAR_TYPE_F32:
        return &value->f32;

    default:
        return RT_NULL;
    }
}

/**
 * @brief Read one scalar parameter through autogen_parameter_manager.
 * @param id External parameter ID mapped from the UDS DID.
 * @param data Buffer to store the native scalar bytes.
 * @param data_size Size of the output buffer in bytes.
 * @param len_out Pointer to the actual scalar byte length.
 * @return UDS_PositiveResponse or an appropriate NRC.
 */
static UDSErr_t helper_param_read(uint16_t id, uint8_t *data, uint16_t data_size, uint16_t *len_out)
{
    par_num_t par_num;
    par_type_t value;
    par_type_list_t type;
    par_status_t status;
    uint16_t len;
    void *value_ptr;
    UDSErr_t nrc;

    if (!par_is_init())
    {
        return UDS_NRC_ConditionsNotCorrect;
    }

    status = par_get_num_by_id(id, &par_num);
    nrc = helper_param_status_to_nrc(status);
    if (nrc != UDS_PositiveResponse)
    {
        return nrc;
    }

    type = par_get_type(par_num);
    nrc = helper_param_scalar_size(type, &len);
    if (nrc != UDS_PositiveResponse)
    {
        return nrc;
    }

    if (len > data_size)
    {
        return UDS_NRC_ResponseTooLong;
    }

    value_ptr = helper_param_scalar_ptr(&value, type);
    if (value_ptr == RT_NULL)
    {
        return UDS_NRC_RequestOutOfRange;
    }

    status = par_get_scalar_by_id(id, value_ptr);
    nrc = helper_param_status_to_nrc(status);
    if (nrc != UDS_PositiveResponse)
    {
        return nrc;
    }

    memcpy(data, value_ptr, len);
    *len_out = len;
    return UDS_PositiveResponse;
}

/**
 * @brief Write one scalar parameter through autogen_parameter_manager.
 * @param id External parameter ID mapped from the UDS DID.
 * @param data Pointer to native scalar bytes.
 * @param size Scalar byte length.
 * @return UDS_PositiveResponse or an appropriate NRC.
 */
static UDSErr_t helper_param_write(uint16_t id, const uint8_t *data, uint16_t size)
{
    par_num_t par_num;
    par_type_t value;
    par_type_list_t type;
    par_status_t status;
    uint16_t len;
    void *value_ptr;
    UDSErr_t nrc;

    if (!par_is_init())
    {
        return UDS_NRC_ConditionsNotCorrect;
    }

    status = par_get_num_by_id(id, &par_num);
    nrc = helper_param_status_to_nrc(status);
    if (nrc != UDS_PositiveResponse)
    {
        return nrc;
    }

    type = par_get_type(par_num);
    nrc = helper_param_scalar_size(type, &len);
    if (nrc != UDS_PositiveResponse)
    {
        return nrc;
    }

    if (size != len)
    {
        return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
    }

    value_ptr = helper_param_scalar_ptr(&value, type);
    if (value_ptr == RT_NULL)
    {
        return UDS_NRC_RequestOutOfRange;
    }

    memcpy(value_ptr, data, len);
    status = par_set_scalar_by_id(id, value_ptr);
    nrc = helper_param_status_to_nrc(status);
    if (nrc != UDS_PositiveResponse)
    {
        return nrc;
    }

#if defined(UDS_PARAM_SAVE_AFTER_WDBI) && (1 == PAR_CFG_NVM_EN)
    status = par_save_by_id(id);
    nrc = helper_param_status_to_nrc(status);
    if (nrc != UDS_PositiveResponse)
    {
        return nrc;
    }
#endif /* defined(UDS_PARAM_SAVE_AFTER_WDBI) && (1 == PAR_CFG_NVM_EN) */

    return UDS_PositiveResponse;
}

/* ==========================================================================
 * UDS Service Handlers
 * ========================================================================== */

/**
 * @brief Handler for Service 0x22 (ReadDataByIdentifier).
 * @details Reads one scalar parameter whose external parameter ID equals the
 *          requested UDS DID. Values are copied as native memory without
 *          byte-order conversion.
 * @param srv UDS Server instance.
 * @param data Pointer to UDSRDBIArgs_t.
 * @param context Unused.
 * @return UDS_PositiveResponse or NRC.
 */
static UDS_HANDLER(handle_rdbi)
{
    UDSRDBIArgs_t *args = (UDSRDBIArgs_t *)data;
    UDSErr_t result;
    uint16_t read_len = 0;
    uint8_t temp_buf[PARAM_RDBI_BUF_SIZE];

    result = helper_param_read(args->dataId, temp_buf, (uint16_t)sizeof(temp_buf), &read_len);
    if (result == UDS_PositiveResponse)
    {
        return args->copy(srv, temp_buf, read_len);
    }

    return result;
}

/**
 * @brief Handler for Service 0x2E (WriteDataByIdentifier).
 * @details Writes one scalar parameter whose external parameter ID equals the
 *          requested UDS DID. Values are interpreted as native memory without
 *          byte-order conversion. When UDS_PARAM_SAVE_AFTER_WDBI is enabled and
 *          parameters NVM support is compiled in, the updated parameter is saved
 *          with par_save_by_id().
 * @param srv UDS Server instance.
 * @param data Pointer to UDSWDBIArgs_t.
 * @param context Unused.
 * @return UDS_PositiveResponse or NRC.
 */
static UDS_HANDLER(handle_wdbi)
{
    UDSWDBIArgs_t *args = (UDSWDBIArgs_t *)data;

    return helper_param_write(args->dataId, args->data, args->len);
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

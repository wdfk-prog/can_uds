/**
 * @file service_0x2A_periodic.c
 * @brief RT-Thread business adapter for UDS 0x2A (ReadDataByPeriodicIdentifier).
 * @details This adapter keeps PDID provider binding and subscription ownership
 *          in business layer, while iso14229 core only performs protocol flow.
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

#define DBG_TAG "uds.0x2a"
#define DBG_LVL DBG_WARNING
#include <rtdbg.h>

#ifdef UDS_ENABLE_0X2A_PERIODIC_SVC

/**
 * @brief Finds one provider slot by PDID.
 * @param svc Service context.
 * @param data_id PDID key.
 * @return Slot index when found, otherwise -1.
 */
static int find_slot_by_data_id(const uds_0x2a_service_t *svc, uint8_t data_id)
{
    for (uint8_t i = 0U; i < svc->slot_count; i++)
    {
        if (svc->slots[i].data_id == data_id)
        {
            return (int)i;
        }
    }

    return -1;
}

/**
 * @brief Returns whether any registered PDID is currently subscribed.
 * @param svc Service context.
 * @return RT_TRUE when at least one slot is subscribed, otherwise RT_FALSE.
 */
static rt_bool_t any_subscribed(const uds_0x2a_service_t *svc)
{
    for (uint8_t i = 0U; i < svc->slot_count; i++)
    {
        if (svc->slots[i].subscribed)
        {
            return RT_TRUE;
        }
    }
    return RT_FALSE;
}

/**
 * @brief Clears all active subscriptions but keeps provider registrations.
 * @param svc Service context.
 */
static void clear_all_subscriptions(uds_0x2a_service_t *svc)
{
    for (uint8_t i = 0U; i < svc->slot_count; i++)
    {
        svc->slots[i].subscribed = RT_FALSE;
    }

    svc->rr_cursor = 0U;
}

/**
 * @brief Removes one provider slot and compacts the array.
 * @param svc Service context.
 * @param idx Slot index.
 */
static void remove_slot_compact(uds_0x2a_service_t *svc, uint8_t idx)
{
    for (uint8_t i = idx; (uint8_t)(i + 1U) < svc->slot_count; i++)
    {
        svc->slots[i] = svc->slots[i + 1U];
    }

    if (svc->slot_count > 0U)
    {
        svc->slot_count--;
        rt_memset(&svc->slots[svc->slot_count], 0, sizeof(svc->slots[svc->slot_count]));
    }

    svc->rr_cursor = 0U;
}

/**
 * @brief Apply subscription updates for UDS 0x2A.
 * @param srv UDS server instance.
 * @param data Pointer to UDSRDBPIApplyArgs_t.
 * @param context Pointer to uds_0x2a_service_t.
 * @return UDS_PositiveResponse or NRC.
 */
static UDS_HANDLER(handle_0x2a_apply)
{
    uds_0x2a_service_t *svc = (uds_0x2a_service_t *)context;
    UDSRDBPIApplyArgs_t *args = (UDSRDBPIApplyArgs_t *)data;
    uds_0x2a_provider_t *slot;
    UDSErr_t err;
    int idx;

    if (svc == RT_NULL || args == RT_NULL)
    {
        return UDS_NRC_ConditionsNotCorrect;
    }

    if (args->stopAll)
    {
        clear_all_subscriptions(svc);
        return UDS_PositiveResponse;
    }

    idx = find_slot_by_data_id(svc, args->periodicDataId);
    if (idx < 0)
    {
        return UDS_NRC_RequestOutOfRange;
    }

    if (args->transmissionMode == UDS_TM_STOP_SENDING)
    {
        svc->slots[idx].subscribed = RT_FALSE;
        svc->rr_cursor = 0U;
        return UDS_PositiveResponse;
    }

    slot = &svc->slots[idx];

    if (slot->check != RT_NULL)
    {
        err = slot->check(srv, slot->data_id, args->transmissionMode, slot->context);
        if (err != UDS_PositiveResponse)
        {
            return err;
        }
    }

    slot->subscribed = RT_TRUE;
    return UDS_PositiveResponse;
}

/**
 * @brief Select and transmit one periodic payload (round-robin).
 * @param srv UDS server instance.
 * @param data Pointer to UDSRDBPITransmitArgs_t.
 * @param context Pointer to uds_0x2a_service_t.
 * @return UDS_PositiveResponse or NRC.
 */
static UDS_HANDLER(handle_0x2a_transmit)
{
    uds_0x2a_service_t *svc = (uds_0x2a_service_t *)context;
    UDSRDBPITransmitArgs_t *args = (UDSRDBPITransmitArgs_t *)data;
    uds_0x2a_provider_t *slot;
    UDSErr_t err;
    uint8_t scanned;
    uint8_t idx;
    uint8_t next_idx;

    if (svc == RT_NULL || args == RT_NULL)
    {
        return UDS_NRC_ConditionsNotCorrect;
    }

    if (svc->slot_count == 0U || !any_subscribed(svc))
    {
        return UDS_NRC_RequestOutOfRange;
    }

    if (svc->rr_cursor >= svc->slot_count)
    {
        svc->rr_cursor = 0U;
    }

    idx = svc->rr_cursor;
    for (scanned = 0U; scanned < svc->slot_count; scanned++)
    {
        slot = &svc->slots[idx];
        next_idx = (uint8_t)((idx + 1U) % svc->slot_count);

        /* Advance cursor after each attempt. */
        svc->rr_cursor = next_idx;

        if (!slot->subscribed)
        {
            idx = next_idx;
            continue;
        }

        args->periodicDataId = slot->data_id;
        args->payload.len = 0U;

        err = slot->read(srv, slot->data_id, args->transmissionMode, &args->payload, slot->context);
        if (err == UDS_PositiveResponse)
        {
            if (args->payload.len > args->payload.bufLen)
            {
                return UDS_NRC_ConditionsNotCorrect;
            }
            return UDS_PositiveResponse;
        }

        return err;
    }

    return UDS_NRC_RequestOutOfRange;
}

/**
 * @brief Register one PDID provider.
 * @param svc Service context.
 * @param provider Provider descriptor.
 * @return RT_EOK on success, otherwise error code.
 */
rt_err_t uds_0x2a_register_provider(uds_0x2a_service_t *svc, const uds_0x2a_provider_t *provider)
{
    uds_0x2a_provider_t *slot;
    int idx;

    if (svc == RT_NULL || provider == RT_NULL || provider->read == RT_NULL)
    {
        return -RT_EINVAL;
    }

    idx = find_slot_by_data_id(svc, provider->data_id);
    if (idx >= 0)
    {
        slot = &svc->slots[idx];
        if (slot->check == provider->check &&
            slot->read == provider->read &&
            slot->context == provider->context)
        {
            return RT_EOK;
        }

        return -RT_EBUSY;
    }

    if (svc->slot_count >= UDS_0X2A_MAX_ACTIVE_PDID)
    {
        return -RT_EFULL;
    }

    slot = &svc->slots[svc->slot_count];
    slot->data_id = provider->data_id;
    slot->check = provider->check;
    slot->read = provider->read;
    slot->context = provider->context;
    slot->subscribed = RT_FALSE;
    svc->slot_count++;

    return RT_EOK;
}

/**
 * @brief Unregister one PDID provider.
 * @param svc Service context.
 * @param data_id PDID value.
 */
void uds_0x2a_unregister_provider(uds_0x2a_service_t *svc, uint8_t data_id)
{
    int idx;

    if (svc == RT_NULL)
    {
        return;
    }

    idx = find_slot_by_data_id(svc, data_id);
    if (idx < 0)
    {
        return;
    }

    remove_slot_compact(svc, (uint8_t)idx);
}

/**
 * @brief Mount 0x2A business service nodes.
 * @param env UDS environment.
 * @param svc Service context.
 * @return RT_EOK on success, otherwise error code.
 */
rt_err_t rtt_uds_0x2a_service_mount(rtt_uds_env_t *env, uds_0x2a_service_t *svc)
{
    const char *apply_name;
    const char *tx_name;
    rt_err_t ret;

    if (env == RT_NULL || svc == RT_NULL)
    {
        return -RT_EINVAL;
    }

    clear_all_subscriptions(svc);

    apply_name = (svc->apply_node.name != RT_NULL) ? svc->apply_node.name : "periodic_apply";
    tx_name = (svc->transmit_node.name != RT_NULL) ? svc->transmit_node.name : "periodic_tx";

    RTT_UDS_SERVICE_NODE_INIT(&svc->apply_node,
                              apply_name,
                              UDS_EVT_ReadPeriodicDataByIdentApply,
                              handle_0x2a_apply,
                              svc,
                              RTT_UDS_PRIO_NORMAL);

    RTT_UDS_SERVICE_NODE_INIT(&svc->transmit_node,
                              tx_name,
                              UDS_EVT_ReadPeriodicDataByIdentTransmit,
                              handle_0x2a_transmit,
                              svc,
                              RTT_UDS_PRIO_NORMAL);

    ret = rtt_uds_service_register(env, &svc->apply_node);
    if (ret != RT_EOK)
    {
        return ret;
    }

    ret = rtt_uds_service_register(env, &svc->transmit_node);
    if (ret != RT_EOK)
    {
        rtt_uds_service_unregister(&svc->apply_node);
        return ret;
    }

    return RT_EOK;
}

/**
 * @brief Unmount 0x2A business service nodes.
 * @param svc Service context.
 */
void rtt_uds_0x2a_service_unmount(uds_0x2a_service_t *svc)
{
    if (svc == RT_NULL)
    {
        return;
    }

    rtt_uds_service_unregister(&svc->apply_node);
    rtt_uds_service_unregister(&svc->transmit_node);
    clear_all_subscriptions(svc);
}

#endif /* UDS_ENABLE_0X2A_PERIODIC_SVC */

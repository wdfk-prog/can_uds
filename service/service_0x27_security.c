/**
 * @file service_0x27_security.c
 * @brief Implementation of UDS Service 0x27 (Security Access).
 * @details Implements Seed & Key logic using a context-based object pattern.
 *          This allows multiple security levels (e.g., Level 1, Level 3) to be
 *          registered as separate instances with different keys/algorithms.
 * 
 * @author wdfk-prog
 * @date 2025-11-29
 * @version 1.0
 */

#include "rtt_uds_service.h"

#define DBG_TAG "uds.sec"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#ifdef UDS_ENABLE_SECURITY_SVC

/* ==========================================================================
 * Internal Helper Functions (Algorithm)
 * ========================================================================== */

/**
 * @brief  Generate a pseudo-random seed.
 * @note   For production, use a True Random Number Generator (TRNG) peripheral.
 * @return 32-bit random seed.
 */
static uint32_t generate_seed(void)
{
    /* Simple PRNG based on system tick. Weak security but sufficient for demo. */
    return (uint32_t)rt_tick_get() ^ 0x12345678;
}

/**
 * @brief  Calculate Key from Seed.
 * @details Default implementation uses simple XOR. 
 *          Replace this with your proprietary algorithm (AES, SAJ1024, etc.).
 * @param  seed Input seed.
 * @param  mask Secret key/mask from the context.
 * @return Calculated key.
 */
static uint32_t calculate_key(uint32_t seed, uint32_t mask)
{
    return seed ^ mask;
}

/* ==========================================================================
 * UDS Service Handlers
 * ========================================================================== */

/**
 * @brief  Handler for Request Seed (0x27 Subfunction Odd).
 */
static UDS_HANDLER(handle_request_seed)
{
    uds_security_service_t *ctx = (uds_security_service_t *)context;
    if (!ctx)
        return UDS_NRC_ConditionsNotCorrect;

    UDSSecAccessRequestSeedArgs_t *args = (UDSSecAccessRequestSeedArgs_t *)data;
    uint8_t seed_buf[4];

    /* 1. Verify SubFunction Level matches this instance */
    if (args->level != ctx->supported_level)
    {
        LOG_W("Invalid SubFunction Level: 0x%02X", args->level);
        return UDS_NRC_SubFunctionNotSupported;
    }

    LOG_I("Request Seed Lvl: 0x%02X", args->level);

    /* 2. Check if already unlocked (Zero Seed Rule) */
    if (srv->securityLevel == args->level)
    {
        /* 
         * ISO 14229-1:2020 Clause 10.4.1: 
         * If the requested security level is already unlocked, 
         * the server shall respond with a seed value equal to zero (0).
         */
        rt_memset(seed_buf, 0, 4);
        LOG_D("Already Unlocked. Sending Zero Seed.");
        return args->copySeed(srv, seed_buf, 4);
    }

    /* 3. Generate New Seed */
    ctx->current_seed = generate_seed();

    /* 4. Serialize (Big Endian) */
    seed_buf[0] = (uint8_t)((ctx->current_seed >> 24) & 0xFF);
    seed_buf[1] = (uint8_t)((ctx->current_seed >> 16) & 0xFF);
    seed_buf[2] = (uint8_t)((ctx->current_seed >> 8) & 0xFF);
    seed_buf[3] = (uint8_t)((ctx->current_seed >> 0) & 0xFF);

    LOG_D("Generated Seed: 0x%08X", ctx->current_seed);

    return args->copySeed(srv, seed_buf, 4);
}

/**
 * @brief  Handler for Send Key (0x27 Subfunction Even).
 */
static UDS_HANDLER(handle_validate_key)
{
    uds_security_service_t *ctx = (uds_security_service_t *)context;
    if (!ctx)
        return UDS_NRC_ConditionsNotCorrect;

    UDSSecAccessValidateKeyArgs_t *args = (UDSSecAccessValidateKeyArgs_t *)data;
    uint32_t received_key = 0;
    uint32_t expected_key = 0;

    /* 1. Verify Level (Must be Seed Level + 1) */
    /* Note: 'args->level' passed here is the target security level (e.g., 1), 
       derived from SubFunc - 1 by the core library. */
    if (args->level != ctx->supported_level)
    {
        return UDS_NRC_SubFunctionNotSupported;
    }

    LOG_I("Validate Key for Lvl: 0x%02X", args->level);

    /* 2. Verify Sequence (Must have requested seed first) */
    if (ctx->current_seed == 0)
    {
        LOG_W("Sequence Error: Key sent without Seed request.");
        return UDS_NRC_RequestSequenceError;
    }

    /* 3. Check Format */
    if (args->len != 4)
    {
        return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
    }

    /* 4. Deserialize Received Key */
    received_key |= (uint32_t)args->key[0] << 24;
    received_key |= (uint32_t)args->key[1] << 16;
    received_key |= (uint32_t)args->key[2] << 8;
    received_key |= (uint32_t)args->key[3] << 0;

    /* 5. Calculate Expected Key */
    expected_key = calculate_key(ctx->current_seed, ctx->secret_key);

    /* 6. Clear Seed (One-time use) */
    ctx->current_seed = 0;

    /* 7. Compare */
    if (received_key == expected_key)
    {
        LOG_I("Security Access Granted!");
        /* The core library will update srv->securityLevel upon PositiveResponse */
        return UDS_PositiveResponse;
    }
    else
    {
        LOG_W("Invalid Key! Recv: %08X, Exp: %08X", received_key, expected_key);
        /* Security delay timer is handled by core library on receiving this NRC */
        return UDS_NRC_InvalidKey;
    }
}

/**
 * @brief  Handler for Session Timeout.
 * @details Resets the internal seed state when session drops to default.
 */
static UDS_HANDLER(handle_sec_session_timeout)
{
    uds_security_service_t *ctx = (uds_security_service_t *)context;

    if (ctx && ctx->current_seed != 0)
    {
        LOG_D("Timeout: Clearing seed state for Lvl 0x%02X", ctx->supported_level);
        ctx->current_seed = 0;
    }

    /* Return CONTINUE to allow other services to handle timeout */
    return RTT_UDS_CONTINUE;
}

/* ==========================================================================
 * Public API Implementation
 * ========================================================================== */

rt_err_t rtt_uds_sec_service_mount(rtt_uds_env_t *env, uds_security_service_t *svc)
{
    rt_err_t ret;

    if (!env || !svc)
        return -RT_EINVAL;

    /* Handle dynamic names if not statically initialized */
    const char *seed_name = svc->req_seed_node.name ? svc->req_seed_node.name : "sec_seed";
    const char *key_name = svc->val_key_node.name ? svc->val_key_node.name : "sec_key";
    const char *tmo_name = svc->timeout_node.name ? svc->timeout_node.name : "sec_tmo";

    /* 
     * Initialize Nodes 
     * Binds the specific event IDs to the specific handlers and context.
     */

    /* Node 1: Request Seed (0x27 Subfunction Odd) */
    /* Note: Core library maps RequestSeed event for ALL odd subfunctions. 
       The handler filters by level. */
    RTT_UDS_SERVICE_NODE_INIT(&svc->req_seed_node,
                              seed_name,
                              UDS_EVT_SecAccessRequestSeed,
                              handle_request_seed,
                              svc,
                              RTT_UDS_PRIO_NORMAL);

    /* Node 2: Validate Key (0x27 Subfunction Even) */
    RTT_UDS_SERVICE_NODE_INIT(&svc->val_key_node,
                              key_name,
                              UDS_EVT_SecAccessValidateKey,
                              handle_validate_key,
                              svc,
                              RTT_UDS_PRIO_NORMAL);

    /* Node 3: Session Timeout */
    RTT_UDS_SERVICE_NODE_INIT(&svc->timeout_node,
                              tmo_name,
                              UDS_EVT_SessionTimeout,
                              handle_sec_session_timeout,
                              svc,
                              RTT_UDS_PRIO_HIGH);

    /* Register to Core */
    ret = rtt_uds_service_register(env, &svc->req_seed_node);
    if (ret != RT_EOK)
        return ret;

    ret = rtt_uds_service_register(env, &svc->val_key_node);
    if (ret != RT_EOK)
        return ret;

    ret = rtt_uds_service_register(env, &svc->timeout_node);

    LOG_D("Security Service Mounted (Lvl 0x%02X)", svc->supported_level);
    return ret;
}

void rtt_uds_sec_service_unmount(uds_security_service_t *svc)
{
    if (!svc)
        return;

    rtt_uds_service_unregister(&svc->req_seed_node);
    rtt_uds_service_unregister(&svc->val_key_node);
    rtt_uds_service_unregister(&svc->timeout_node);
}

#endif /* UDS_ENABLE_SECURITY_SVC */

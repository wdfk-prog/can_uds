/**
 * @file rtt_uds_service.h
 * @brief Helper macros and structures for defining UDS services in RT-Thread.
 * @details This file facilitates the registration and management of UDS service handlers.
 *          It includes:
 *          - Auto-Registration macros (declare/define pairs).
 *          - Specific service context definitions (IO Control 0x2F, Comm Control 0x28).
 *          - Public API declarations for service lifecycle management.
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
#ifndef __RTT_UDS_SERVICE_H__
#define __RTT_UDS_SERVICE_H__

#include "iso14229_rtt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Standard Service Auto-Registration Declarations
 * ========================================================================== */

#ifdef UDS_ENABLE_SESSION_SVC
RTT_UDS_SERVICE_DECLARE(session_control_node);
#endif //UDS_ENABLE_SESSION_SVC

/* ==========================================================================
 * Service 0x27: Security Access
 * ========================================================================== */

#ifdef UDS_ENABLE_SECURITY_SVC

/**
 * @brief Security-access service context.
 * @details Stores configuration (supported level and secret key), current seed,
 *          and the internal service nodes used by SID 0x27.
 */
typedef struct
{
    /* --- Configuration (Set during init) --- */
    uint8_t supported_level;   /**< The security level managed by this instance (e.g., 0x01) */
    uint32_t secret_key;        /**< Secret key mask for algorithm */

    /* --- Runtime State --- */
    uint32_t current_seed;      /**< Current seed waiting for validation (0 = none) */

    /* --- Internal Nodes (Registered to Core) --- */
    uds_service_node_t req_seed_node;
    uds_service_node_t val_key_node;
    uds_service_node_t timeout_node;
} uds_security_service_t;

/* --- Macros for Static Definition (Compile-time) --- */

/**
 * @brief  Statically define a Security Service Instance.
 * @param _name  Variable name.
 * @param _lvl   Security Level (e.g., 0x01).
 * @param _key   Secret Key (e.g., 0xA5A5A5A5).
 */
#define RTT_UDS_SEC_SERVICE_DEFINE(_name, _lvl, _key)                                                                       \
    static uds_security_service_t _name = {                                                                                 \
        .supported_level = _lvl,                                                                                            \
        .secret_key = _key,                                                                                                 \
        .current_seed = 0,                                                                                                  \
        .req_seed_node = {                                                                                                  \
            .list = RT_LIST_OBJECT_INIT(_name.req_seed_node.list),                                                          \
            .name = #_name "_seed",                                                                                         \
            .context = &_name },                                                                                            \
        .val_key_node = { .list = RT_LIST_OBJECT_INIT(_name.val_key_node.list), .name = #_name "_key", .context = &_name }, \
        .timeout_node = { .list = RT_LIST_OBJECT_INIT(_name.timeout_node.list), .name = #_name "_tmo", .context = &_name }  \
    }

/* --- Macros for Runtime Initialization (Dynamic/Stack) --- */

/**
 * @brief  Runtime initialization for Security Service Instance.
 * @param _svc_ptr  Pointer to struct.
 * @param _name_str Debug name string prefix.
 * @param _lvl      Security Level.
 * @param _key      Secret Key.
 */
#define RTT_UDS_SEC_SERVICE_INIT(_svc_ptr, _name_str, _lvl, _key) \
    do                                                            \
    {                                                             \
        (_svc_ptr)->supported_level = (_lvl);                     \
        (_svc_ptr)->secret_key = (_key);                          \
        (_svc_ptr)->current_seed = 0;                             \
        /* Req Seed Node */                                       \
        rt_list_init(&(_svc_ptr)->req_seed_node.list);            \
        (_svc_ptr)->req_seed_node.name = _name_str "_seed";       \
        (_svc_ptr)->req_seed_node.context = (_svc_ptr);           \
        (_svc_ptr)->req_seed_node.priority = RTT_UDS_PRIO_NORMAL; \
        /* Val Key Node */                                        \
        rt_list_init(&(_svc_ptr)->val_key_node.list);             \
        (_svc_ptr)->val_key_node.name = _name_str "_key";         \
        (_svc_ptr)->val_key_node.context = (_svc_ptr);            \
        (_svc_ptr)->val_key_node.priority = RTT_UDS_PRIO_NORMAL;  \
        /* Timeout Node */                                        \
        rt_list_init(&(_svc_ptr)->timeout_node.list);             \
        (_svc_ptr)->timeout_node.name = _name_str "_tmo";         \
        (_svc_ptr)->timeout_node.context = (_svc_ptr);            \
        (_svc_ptr)->timeout_node.priority = RTT_UDS_PRIO_HIGH;    \
    } while (0)

/* --- API --- */

/**
 * @brief Mount one security-access service instance.
 * @param env UDS environment.
 * @param svc Security service context.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t rtt_uds_sec_service_mount(rtt_uds_env_t *env, uds_security_service_t *svc);

/**
 * @brief Unmount one security-access service instance.
 * @param svc Security service context.
 */
void rtt_uds_sec_service_unmount(uds_security_service_t *svc);

#endif /* UDS_ENABLE_SECURITY_SVC */

#ifdef UDS_ENABLE_PARAM_SVC
RTT_UDS_SERVICE_DECLARE(param_rdbi_node);
RTT_UDS_SERVICE_DECLARE(param_wdbi_node);
#endif //UDS_ENABLE_PARAM_SVC

#ifdef UDS_ENABLE_CONSOLE_SVC

#ifndef UDS_CONSOLE_BUF_SIZE
#define UDS_CONSOLE_BUF_SIZE 4000
#endif

#ifndef UDS_CONSOLE_CMD_BUF_SIZE
#define UDS_CONSOLE_CMD_BUF_SIZE 128
#endif

/**
 * @brief Console Service Context
 * @details Encapsulates the Virtual UART Device and the UDS Service Node.
 */
typedef struct
{
    /* --- RT-Thread Device Object (Must be first for polymorphism) --- */
    struct rt_device dev;

    /* --- Runtime State --- */
    char buffer[UDS_CONSOLE_BUF_SIZE]; /**< Capture buffer (Static BSS) */
    rt_size_t pos;                     /**< Write position */
    rt_bool_t overflow;                /**< Overflow flag */
    rt_device_t old_console;           /**< Saved previous console */

    /* --- Configuration --- */
    const char *dev_name;              /**< Device registration name (e.g. "uds_vcon") */

    /* --- Internal Service Node --- */
    uds_service_node_t service_node;   /**< 0x31 Handler Node */
} uds_console_service_t;

/* --- Macros for Static Definition --- */

/**
 * @brief  Runtime initialization for Console Service Instance.
 * 
 * @param _svc_ptr      Pointer to the service struct.
 * @param _name_str     Service Node Name (e.g., "console").
 * @param _dev_name_str RT-Thread Device Name (e.g., "uds_vcon").
 */
#define RTT_UDS_CONSOLE_SERVICE_INIT(_svc_ptr, _name_str, _dev_name_str) \
    do                                                                   \
    {                                                                    \
        rt_memset((_svc_ptr), 0, sizeof(uds_console_service_t));         \
        (_svc_ptr)->dev_name = (_dev_name_str);                          \
        rt_list_init(&(_svc_ptr)->service_node.list);                    \
        (_svc_ptr)->service_node.name = (_name_str);                     \
        (_svc_ptr)->service_node.context = (_svc_ptr);                   \
    } while (0)

/**
 * @brief  Statically define a Console Service Instance.
 * @param _name      Variable name.
 * @param _dev_name  RT-Thread Device Name string (e.g., "uds_vcon").
 */
#define RTT_UDS_CONSOLE_SERVICE_DEFINE(_name, _dev_name)          \
    static uds_console_service_t _name = {                        \
        .dev_name = _dev_name,                                    \
        .pos = 0,                                                 \
        .overflow = RT_FALSE,                                     \
        .old_console = RT_NULL,                                   \
        .service_node = {                                         \
            .list = RT_LIST_OBJECT_INIT(_name.service_node.list), \
            .name = #_name,                                       \
            .context = &_name }                                   \
    }

/* --- API --- */

/**
 * @brief Mount one remote-console service instance.
 * @details Registers the virtual capture device and binds the 0x31 routine node.
 * @param env UDS environment.
 * @param svc Console service context.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t rtt_uds_console_service_mount(rtt_uds_env_t *env, uds_console_service_t *svc);

/**
 * @brief Unmount one remote-console service instance.
 * @details Unregisters the virtual device and removes the routine handler node.
 * @param svc Console service context.
 */
void rtt_uds_console_service_unmount(uds_console_service_t *svc);
#endif //UDS_ENABLE_CONSOLE_SVC

#ifdef UDS_ENABLE_FILE_SVC

#ifndef UDS_FILE_MAX_PATH_LEN
#define UDS_FILE_MAX_PATH_LEN 64
#endif

/**
 * @brief File-transfer session mode.
 */
typedef enum
{
    FILE_MODE_IDLE = 0, /**< No active transfer session. */
    FILE_MODE_WRITING,  /**< Client uploads data to the ECU. */
    FILE_MODE_READING   /**< ECU uploads data back to the client. */
} uds_file_mode_t;

/**
 * @brief File Service Context
 * @details Stores the state of the current file transfer session.
 *          Support instantiation to allow multiple contexts (though UDS usually runs one).
 */
typedef struct
{
    /* Runtime State */
    int fd;                 /**< File Descriptor */
    uint32_t total_size;    /**< Expected total size */
    uint32_t current_pos;   /**< Current read/write offset */
    uds_file_mode_t mode;   /**< Current transfer state */
    char current_path[UDS_FILE_MAX_PATH_LEN];  /**< Path of the file */
    uint32_t current_crc;   /**< Running CRC32 */

    /* Service Nodes */
    uds_service_node_t req_node;     /* 0x38 RequestFileTransfer */
    uds_service_node_t data_node;    /* 0x36 TransferData */
    uds_service_node_t exit_node;    /* 0x37 RequestTransferExit */
    uds_service_node_t timeout_node; /* Session Timeout Handler */
} uds_file_service_t;

/* --- Macros for Static Definition --- */

/**
 * @brief  Runtime initialization for File Service Instance.
 * @details Clears the structure, resets FD, and initializes list nodes/names.
 * 
 * @param _svc_ptr  Pointer to the service struct.
 * @param _name_str Base name string (e.g., "file_svc").
 */
#define RTT_UDS_FILE_SERVICE_INIT(_svc_ptr, _name_str)        \
    do                                                        \
    {                                                         \
        rt_memset((_svc_ptr), 0, sizeof(uds_file_service_t)); \
        (_svc_ptr)->fd = -1;                                  \
        (_svc_ptr)->mode = FILE_MODE_IDLE;                    \
        rt_list_init(&(_svc_ptr)->req_node.list);             \
        (_svc_ptr)->req_node.name = _name_str "_req";         \
        (_svc_ptr)->req_node.context = (_svc_ptr);            \
        rt_list_init(&(_svc_ptr)->data_node.list);            \
        (_svc_ptr)->data_node.name = _name_str "_data";       \
        (_svc_ptr)->data_node.context = (_svc_ptr);           \
        rt_list_init(&(_svc_ptr)->exit_node.list);            \
        (_svc_ptr)->exit_node.name = _name_str "_exit";       \
        (_svc_ptr)->exit_node.context = (_svc_ptr);           \
        rt_list_init(&(_svc_ptr)->timeout_node.list);         \
        (_svc_ptr)->timeout_node.name = _name_str "_tmo";     \
        (_svc_ptr)->timeout_node.context = (_svc_ptr);        \
    } while (0)

/**
 * @brief  Statically define a File Service Instance.
 * @param _name Name of the variable.
 */
#define RTT_UDS_FILE_SERVICE_DEFINE(_name)                                                                                 \
    static uds_file_service_t _name = {                                                                                    \
        .fd = -1,                                                                                                          \
        .mode = FILE_MODE_IDLE,                                                                                            \
        .req_node = {                                                                                                      \
            .list = RT_LIST_OBJECT_INIT(_name.req_node.list),                                                              \
            .name = #_name "_req",                                                                                         \
            .context = &_name },                                                                                           \
        .data_node = { .list = RT_LIST_OBJECT_INIT(_name.data_node.list), .name = #_name "_data", .context = &_name },     \
        .exit_node = { .list = RT_LIST_OBJECT_INIT(_name.exit_node.list), .name = #_name "_exit", .context = &_name },     \
        .timeout_node = { .list = RT_LIST_OBJECT_INIT(_name.timeout_node.list), .name = #_name "_tmo", .context = &_name } \
    }

/* --- API --- */

/**
 * @brief Mount one file-transfer service instance.
 * @param env UDS environment.
 * @param svc File service context.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t rtt_uds_file_service_mount(rtt_uds_env_t *env, uds_file_service_t *svc);

/**
 * @brief Unmount one file-transfer service instance.
 * @param svc File service context.
 */
void rtt_uds_file_service_unmount(uds_file_service_t *svc);
#endif //UDS_ENABLE_FILE_SVC

#ifdef UDS_ENABLE_0X11_RESET_SVC
RTT_UDS_SERVICE_DECLARE(reset_exec_node);
RTT_UDS_SERVICE_DECLARE(reset_req_node);
#endif //UDS_ENABLE_0X11_RESET_SVC

/* ==========================================================================
 * Service 0x2A: ReadDataByPeriodicIdentifier (Business Adapter)
 * ========================================================================== */

#ifdef UDS_ENABLE_0X2A_PERIODIC_SVC

#ifndef UDS_0X2A_MAX_ACTIVE_PDID
#define UDS_0X2A_MAX_ACTIVE_PDID 8
#endif

#if (UDS_0X2A_MAX_ACTIVE_PDID < 1)
#error "UDS_0X2A_MAX_ACTIVE_PDID must be >= 1"
#endif

/**
 * @brief 0x2A provider access-check callback type.
 * @details Called during apply phase for non-stop requests before a PDID is subscribed.
 *          Return UDS_PositiveResponse when the PDID can be scheduled, otherwise return NRC.
 *
 * @param srv UDS server instance.
 * @param data_id PDID value (F2xx low byte).
 * @param transmission_mode Requested transmission mode.
 * @param context Provider private context.
 * @return UDS_PositiveResponse on success, otherwise NRC.
 */
typedef UDSErr_t (*uds_0x2a_access_check_t)(UDSServer_t *srv, uint8_t data_id, UDSRDBPITransmissionMode_t transmission_mode, void *context);

/**
 * @brief 0x2A provider payload callback type.
 * @details Called during periodic transmit phase. The provider fills @p payload.
 *
 * @param srv UDS server instance.
 * @param data_id PDID value (F2xx low byte).
 * @param transmission_mode Active transmission mode.
 * @param payload Output payload buffer.
 * @param context Provider private context.
 * @return UDS_PositiveResponse when payload is ready, otherwise NRC.
 */
typedef UDSErr_t (*uds_0x2a_read_data_t)(UDSServer_t *srv, uint8_t data_id, UDSRDBPITransmissionMode_t transmission_mode, UDSPayloadBuffer_t *payload, void *context);

/**
 * @brief Provider descriptor for one PDID.
 * @details This structure is also used as one slot entry in service context.
 */
typedef struct
{
    uint8_t data_id;                /**< PDID value (F2xx low byte) */
    uds_0x2a_access_check_t check;  /**< optional access/session/security check callback */
    uds_0x2a_read_data_t read;      /**< payload callback, must not be NULL */
    void *context;                  /**< user private context */
    rt_bool_t subscribed;           /**< runtime subscription flag maintained by service */
} uds_0x2a_provider_t;

/**
 * @brief 0x2A business-layer service context.
 * @details Keeps provider bindings and active subscriptions outside iso14229 core.
 */
typedef struct
{
    uds_0x2a_provider_t slots[UDS_0X2A_MAX_ACTIVE_PDID]; /**< provider/subscription slots */
    uint8_t slot_count;                                   /**< number of registered provider slots */
    uint8_t rr_cursor;                                    /**< round-robin cursor for transmit selection */
    uds_service_node_t apply_node;                        /**< event node for UDS_EVT_ReadPeriodicDataByIdentApply */
    uds_service_node_t transmit_node;                     /**< event node for UDS_EVT_ReadPeriodicDataByIdentTransmit */
} uds_0x2a_service_t;

/**
 * @brief Runtime initialization for 0x2A business service instance.
 *
 * @param _svc_ptr Pointer to service context.
 * @param _name_str Base debug name (for node names).
 */
#define RTT_UDS_0X2A_SERVICE_INIT(_svc_ptr, _name_str)            \
    do                                                            \
    {                                                             \
        rt_memset((_svc_ptr), 0, sizeof(uds_0x2a_service_t));     \
        rt_list_init(&(_svc_ptr)->apply_node.list);               \
        (_svc_ptr)->apply_node.name = _name_str "_2a_apply";      \
        (_svc_ptr)->apply_node.context = (_svc_ptr);              \
        (_svc_ptr)->apply_node.priority = RTT_UDS_PRIO_NORMAL;    \
        rt_list_init(&(_svc_ptr)->transmit_node.list);            \
        (_svc_ptr)->transmit_node.name = _name_str "_2a_tx";      \
        (_svc_ptr)->transmit_node.context = (_svc_ptr);           \
        (_svc_ptr)->transmit_node.priority = RTT_UDS_PRIO_NORMAL; \
    } while (0)

/**
 * @brief Statically define a 0x2A business service instance.
 *
 * @param _name Variable name.
 */
#define RTT_UDS_0X2A_SERVICE_DEFINE(_name)                                                                                     \
    static uds_0x2a_service_t _name = {                                                                                        \
        .slots = { 0 },                                                                                                        \
        .slot_count = 0,                                                                                                       \
        .rr_cursor = 0,                                                                                                        \
        .apply_node = {                                                                                                        \
            .list = RT_LIST_OBJECT_INIT(_name.apply_node.list),                                                                \
            .name = #_name "_2a_apply",                                                                                        \
            .context = &_name },                                                                                               \
        .transmit_node = { .list = RT_LIST_OBJECT_INIT(_name.transmit_node.list), .name = #_name "_2a_tx", .context = &_name } \
    }

/**
 * @brief Register one periodic-data provider.
 * @details The provider is keyed by PDID. Duplicate registration with different callbacks is rejected.
 * @param svc 0x2A service context.
 * @param provider Provider descriptor.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t uds_0x2a_register_provider(uds_0x2a_service_t *svc, const uds_0x2a_provider_t *provider);

/**
 * @brief Unregister one periodic-data provider by PDID.
 * @details If the PDID is active, the subscription state is cleared together with the provider slot.
 * @param svc 0x2A service context.
 * @param data_id Periodic data identifier.
 */
void uds_0x2a_unregister_provider(uds_0x2a_service_t *svc, uint8_t data_id);

/**
 * @brief Mount one 0x2A periodic-data service instance.
 * @param env UDS environment.
 * @param svc 0x2A service context.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t rtt_uds_0x2a_service_mount(rtt_uds_env_t *env, uds_0x2a_service_t *svc);

/**
 * @brief Unmount one 0x2A periodic-data service instance.
 * @param svc 0x2A service context.
 */
void rtt_uds_0x2a_service_unmount(uds_0x2a_service_t *svc);

#endif /* UDS_ENABLE_0X2A_PERIODIC_SVC */

/* ==========================================================================
 * Service 0x2F: InputOutputControlByIdentifier (IO Control)
 * ========================================================================== */

#ifdef UDS_ENABLE_0X2F_IO_SVC

/** 
 * @brief Max length of IO Control Status Record in bytes.
 * @details Adjust based on project needs (e.g., VIN might need 17+ bytes).
 *          Default 32 bytes is sufficient for most sensors/actuators.
 */
#ifndef UDS_IO_MAX_RESP_LEN
#define UDS_IO_MAX_RESP_LEN 32 /* Kconfig Default: 32 */
#endif

/**
 * @brief UDS 0x2F InputOutputControlParameter (IOCP) Actions.
 * @details ISO 14229-1:2020 Table 417 - Definition of inputOutputControlParameter values.
 */
typedef enum
{
    /** 
     * @brief ReturnControlToECU (0x00)
     * The server shall return control of the input/output signal to the internal logic.
     */
    UDS_IO_ACT_RETURN_CONTROL = UDS_IOCP_RET_CTRL_TO_ECU,

    /** 
     * @brief ResetToDefault (0x01)
     * The server shall set the input/output signal to its default value.
     */
    UDS_IO_ACT_RESET_TO_DEFAULT = UDS_IOCP_RESET_TO_DEFAULT,

    /** 
     * @brief FreezeCurrentState (0x02)
     * The server shall freeze the input/output signal at its current value.
     */
    UDS_IO_ACT_FREEZE_CURRENT = UDS_IOCP_FREEZE_CUR_STATE,

    /** 
     * @brief ShortTermAdjustment (0x03)
     * The server shall set the input/output signal to the value provided in the controlState parameter.
     */
    UDS_IO_ACT_SHORT_TERM_ADJ = UDS_IOCP_SHORT_TERM_ADJ

} uds_io_action_t;

/**
 * @brief User Callback for IO Operations.
 * 
 * @param did       Data Identifier being accessed.
 * @param action    Requested action (Freeze, Reset, Adjust, etc.).
 * @param input     ControlState/Mask buffer (Valid only for SHORT_TERM_ADJ).
 * @param input_len Length of input data.
 * @param out_buf   [Output] Buffer to store ControlStatusRecord.
 * @param out_len   [In/Out] 
 *                  In: Max capacity of out_buf (UDS_IO_MAX_RESP_LEN).
 *                  Out: Actual bytes written by user.
 * 
 * @return UDS_PositiveResponse on success, or an NRC error code.
 */
typedef UDSErr_t (*uds_io_handler_t)(uint16_t did,
                                     uds_io_action_t action,
                                     const void *input,
                                     size_t input_len,
                                     void *out_buf,
                                     size_t *out_len);

/**
 * @brief IO Node Control Block.
 * @details Represents a single hardware point (DID) managed by the IO service.
 *          Users should define this structure statically using RTT_UDS_IO_NODE_DEFINE.
 */
typedef struct uds_io_node
{
    rt_list_t list;             /**< Internal list node */
    uint16_t did;               /**< DID managed by this node (e.g., 0x0100) */
    uds_io_handler_t handler;   /**< User callback */

    /* Runtime Status (Managed by Framework) */
    uint8_t is_overridden;      /**< Flag: 1=Controlled by UDS, 0=App Control */
} uds_io_node_t;

/**
 * @brief IO Service Context.
 * @details Manages a collection of IO nodes and handles UDS dispatching.
 *          Supports multiple instances if needed.
 */
typedef struct
{
    rt_list_t node_list; /**< List head for registered user IO nodes */

    /* 
     * Internal UDS Service Nodes.
     * These hook into the core UDS dispatcher (rtt_uds_env).
     * Must be part of the instance, not global.
     */
    uds_service_node_t ctrl_service_node;    /**< Handles 0x2F requests */
    uds_service_node_t timeout_service_node; /**< Handles SessionTimeout (Reset IO) */
} uds_io_service_t;

/* --- Macros for Static Definition --- */

/**
 * @brief  Statically define an IO Service Instance.
 * @details Initializes list heads and internal service nodes.
 *          Auto-generates names: "_name_ctrl" and "_name_timeout".
 * 
 * @param _name Name of the variable.
 */
#define RTT_UDS_IO_SERVICE_DEFINE(_name)                                                                                                       \
    static uds_io_service_t _name = {                                                                                                          \
        .node_list = RT_LIST_OBJECT_INIT(_name.node_list),                                                                                     \
        .ctrl_service_node = {                                                                                                                 \
            .list = RT_LIST_OBJECT_INIT(_name.ctrl_service_node.list),                                                                         \
            .name = #_name "_ctrl",                                                                                                            \
            .context = &_name },                                                                                                               \
        .timeout_service_node = { .list = RT_LIST_OBJECT_INIT(_name.timeout_service_node.list), .name = #_name "_timeout", .context = &_name } \
    }

/**
 * @brief  Runtime initialization for IO Service Instance.
 * @details Useful for dynamic allocation or stack variables.
 *          Handlers are bound automatically during mount.
 * 
 * @param _svc_ptr  Pointer to the service struct.
 * @param _name_str Base name string (e.g., "io_svc").
 */
#define RTT_UDS_IO_SERVICE_INIT(_svc_ptr, _name_str)                   \
    do                                                                 \
    {                                                                  \
        rt_list_init(&(_svc_ptr)->node_list);                          \
        /* Ctrl Node Init */                                           \
        rt_list_init(&(_svc_ptr)->ctrl_service_node.list);             \
        (_svc_ptr)->ctrl_service_node.name = _name_str "_ctrl";        \
        (_svc_ptr)->ctrl_service_node.context = (_svc_ptr);            \
        (_svc_ptr)->ctrl_service_node.priority = RTT_UDS_PRIO_NORMAL;  \
        /* Timeout Node Init */                                        \
        rt_list_init(&(_svc_ptr)->timeout_service_node.list);          \
        (_svc_ptr)->timeout_service_node.name = _name_str "_timeout";  \
        (_svc_ptr)->timeout_service_node.context = (_svc_ptr);         \
        (_svc_ptr)->timeout_service_node.priority = RTT_UDS_PRIO_HIGH; \
    } while (0)

/**
 * @brief  Statically define an IO Node.
 * @param _name    Variable name.
 * @param _did     DID to handle.
 * @param _handler User callback function.
 */
#define RTT_UDS_IO_NODE_DEFINE(_name, _did, _handler) \
    static uds_io_node_t _name = {                    \
        .list = RT_LIST_OBJECT_INIT(_name.list),      \
        .did = _did,                                  \
        .handler = _handler,                          \
        .is_overridden = 0                            \
    }

/**
 * @brief  Runtime initialization for an IO Node.
 */
#define RTT_UDS_IO_NODE_INIT(_node_ptr, _did, _handler) \
    do                                                  \
    {                                                   \
        rt_list_init(&(_node_ptr)->list);               \
        (_node_ptr)->did = (_did);                      \
        (_node_ptr)->handler = (_handler);              \
        (_node_ptr)->is_overridden = 0;                 \
    } while (0)

/* --- Public API --- */

/**
 * @brief Register one IO-control node.
 * @details The node becomes part of the service-owned DID dispatch table.
 * @param svc IO service context.
 * @param node Node descriptor.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t uds_io_register_node(uds_io_service_t *svc, uds_io_node_t *node);

/**
 * @brief Unregister one IO-control node.
 * @details This only detaches the descriptor. It does not force hardware back to the default state.
 * @param svc IO service context.
 * @param node Node descriptor.
 */
void uds_io_unregister_node(uds_io_service_t *svc, uds_io_node_t *node);

/**
 * @brief Mount one IO-control service instance.
 * @param env UDS environment.
 * @param svc IO service context.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t rtt_uds_io_service_mount(rtt_uds_env_t *env, uds_io_service_t *svc);

/**
 * @brief Unmount one IO-control service instance.
 * @param svc IO service context.
 */
void rtt_uds_io_service_unmount(uds_io_service_t *svc);

/**
 * @brief Query whether a DID is currently overridden by 0x2F control.
 * @param svc IO service context.
 * @param did Data identifier.
 * @return 1 when overridden, 0 when not overridden, -1 when DID is not found.
 */
int uds_io_is_did_overridden(uds_io_service_t *svc, uint16_t did);

#endif /* UDS_ENABLE_0X2F_IO_SVC */

/* ==========================================================================
 * Service 0x28: CommunicationControl
 * ========================================================================== */

#ifdef UDS_ENABLE_0X28_COMM_CTRL_SVC

/**
 * @brief 0x28 Service Context Structure.
 */
typedef struct
{
    uint16_t node_id;                     /**< Local Node ID for addressing checks */
    uds_service_node_t ctrl_service_node; /**< Internal service node for 0x28 */
} uds_comm_ctrl_service_t;

/* --- Macros for Static Definition --- */

/**
 * @brief  Statically define a 0x28 Service Instance.
 * @param _name Variable name.
 * @param _id   Node ID (Constant).
 */
#define RTT_UDS_COMM_CTRL_SERVICE_DEFINE(_name, _id)                   \
    static uds_comm_ctrl_service_t _name = {                           \
        .node_id = _id,                                                \
        .ctrl_service_node = {                                         \
            .list = RT_LIST_OBJECT_INIT(_name.ctrl_service_node.list), \
            .name = #_name,                                            \
            .context = &_name }                                        \
    }

/**
 * @brief  Runtime initialization for 0x28 Service Instance.
 * @param _svc_ptr  Pointer to object.
 * @param _name_str Name string (e.g., "cc_svc").
 * @param _id       Node ID.
 */
#define RTT_UDS_COMM_CTRL_SERVICE_INIT(_svc_ptr, _name_str, _id)      \
    do                                                                \
    {                                                                 \
        (_svc_ptr)->node_id = (_id);                                  \
        rt_list_init(&(_svc_ptr)->ctrl_service_node.list);            \
        (_svc_ptr)->ctrl_service_node.name = _name_str;               \
        (_svc_ptr)->ctrl_service_node.context = (_svc_ptr);           \
        (_svc_ptr)->ctrl_service_node.priority = RTT_UDS_PRIO_NORMAL; \
    } while (0)

/* --- Public API --- */

/**
 * @brief Set the communication-control service node identifier.
 * @param svc Communication-control service context.
 * @param node_id Application-defined node identifier.
 */
void rtt_uds_comm_ctrl_set_id(uds_comm_ctrl_service_t *svc, uint16_t node_id);

/**
 * @brief Mount one communication-control service instance.
 * @param env UDS environment.
 * @param svc Communication-control service context.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t rtt_uds_comm_ctrl_service_mount(rtt_uds_env_t *env, uds_comm_ctrl_service_t *svc);

/**
 * @brief Unmount one communication-control service instance.
 * @param svc Communication-control service context.
 */
void rtt_uds_comm_ctrl_service_unmount(uds_comm_ctrl_service_t *svc);

#endif /* UDS_ENABLE_0X28_COMM_CTRL_SVC */

#ifdef __cplusplus
}
#endif

#endif /* __RTT_UDS_SERVICE_H__ */

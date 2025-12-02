/**
 * @file rtt_uds_example.c
 * @brief Application layer combining UDS framework and hardware control (RT-Thread).
 * @details This file demonstrates how to use the iso14229_rtt library to create
 * a UDS server, register a specific service and bind it to an RT-Thread CAN device.
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
#include "rtt_uds_service.h"

/* Logger configuration */
#define DBG_TAG "uds.ex"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/* ==========================================================================
 * Configuration Macros
 * ========================================================================== */

/* CAN ID Configuration */
#ifndef UDS_ISO_CAN_ID_PHYS
#define UDS_ISO_CAN_ID_PHYS   0x7E0  /**< Physical Request ID (Client -> Server) */
#endif

#ifndef UDS_ISO_CAN_ID_FUNC
#define UDS_ISO_CAN_ID_FUNC   0x7DF  /**< Functional Request ID (Broadcast) */
#endif

#ifndef UDS_ISO_CAN_ID_RESP
#define UDS_ISO_CAN_ID_RESP   0x7E8  /**< Response ID (Server -> Client) */
#endif

/* Thread Configuration */
#ifndef UDS_THREAD_STACK_SIZE
#define UDS_THREAD_STACK_SIZE 4096   /**< Stack size for UDS server thread */
#endif

#ifndef UDS_THREAD_PRIORITY
#define UDS_THREAD_PRIORITY   2      /**< Priority for UDS server thread */
#endif

#ifndef UDS_MSG_QUEUE_SIZE
#define UDS_MSG_QUEUE_SIZE    32     /**< Size of RX message queue */
#endif

/* ==========================================================================
 * Configuration & Defaults
 * ========================================================================== */

/* Default Pin Definitions (Prevent compilation errors if not defined in board.h) */
#ifndef UDS_EXAMPLE_PIN_LED_R
#define UDS_EXAMPLE_PIN_LED_R -1
#endif
#ifndef UDS_EXAMPLE_PIN_LED_G
#define UDS_EXAMPLE_PIN_LED_G -1
#endif
#ifndef UDS_EXAMPLE_PIN_LED_B
#define UDS_EXAMPLE_PIN_LED_B -1
#endif

#ifndef UDS_EXAMPLE_LED_CTRL_DID
#define UDS_EXAMPLE_LED_CTRL_DID 0x0100
#endif

#ifdef UDS_ENABLE_SECURITY_SVC
#ifndef UDS_SEC_DEFAULT_LEVEL
#define UDS_SEC_DEFAULT_LEVEL 0x01
#endif

#ifndef UDS_SEC_DEFAULT_KEY
#define UDS_SEC_DEFAULT_KEY   0xA5A5A5A5
#endif
RTT_UDS_SEC_SERVICE_DEFINE(security_service, UDS_SEC_DEFAULT_LEVEL, UDS_SEC_DEFAULT_KEY);
#endif

#ifdef UDS_ENABLE_0X28_COMM_CTRL_SVC
RTT_UDS_COMM_CTRL_SERVICE_DEFINE(comm_ctrl_service, UDS_COMM_CTRL_ID);
#endif

/* [Modified] File Service Instance */
#ifdef UDS_ENABLE_FILE_SVC
RTT_UDS_FILE_SERVICE_DEFINE(file_service);
#endif

#ifdef UDS_ENABLE_CONSOLE_SVC
#ifndef UDS_CONSOLE_DEV_NAME
#define UDS_CONSOLE_DEV_NAME "uds_vcon"
#endif
RTT_UDS_CONSOLE_SERVICE_DEFINE(console_service, UDS_CONSOLE_DEV_NAME);
#endif

/* ==========================================================================
 * Type Definitions & Globals
 * ========================================================================== */

#ifdef UDS_ENABLE_0X2F_IO_SVC

/** @brief RGB Color Helper Structure */
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

/** @brief Global timer for LED application logic */
static rt_timer_t led_timer = RT_NULL;

/** @brief Definition of the IO service instance */
RTT_UDS_IO_SERVICE_DEFINE(led_io_service);

/* Application State Variables */
static rgb_color_t app_rgb = { 0, 0, 0 }; /**< Target value calculated by App logic */
static rgb_color_t act_rgb = { 0, 0, 0 }; /**< Actual value currently on Hardware */

#endif /* UDS_ENABLE_0X2F_IO_SVC */

/** @brief Global UDS environment handle */
static rtt_uds_env_t *uds_env = RT_NULL;

/** @brief Storage for the original CAN receive callback to restore on stop */
static rt_err_t (*old_can_rx_indicate)(rt_device_t dev, rt_size_t size) = RT_NULL;

/* ==========================================================================
 * Hardware Abstraction Layer
 * ========================================================================== */

#ifdef UDS_ENABLE_0X2F_IO_SVC

/**
 * @brief Write RGB values to physical hardware pins.
 * @param color The RGB color struct containing high/low states (0 or 1).
 */
static void hw_write_leds(rgb_color_t color)
{
    act_rgb = color;

#if (UDS_EXAMPLE_PIN_LED_R != -1)
    rt_pin_write(UDS_EXAMPLE_PIN_LED_R, color.r ? PIN_HIGH : PIN_LOW);
#endif
#if (UDS_EXAMPLE_PIN_LED_G != -1)
    rt_pin_write(UDS_EXAMPLE_PIN_LED_G, color.g ? PIN_HIGH : PIN_LOW);
#endif
#if (UDS_EXAMPLE_PIN_LED_B != -1)
    rt_pin_write(UDS_EXAMPLE_PIN_LED_B, color.b ? PIN_HIGH : PIN_LOW);
#endif
}

/* ==========================================================================
 * Application Business Logic
 * ========================================================================== */

/**
 * @brief LED periodic blinking task (Simulates application logic).
 * @details This function runs periodically. It calculates the desired LED state
 *          but only writes to hardware if UDS has NOT taken control via service 0x2F.
 * @param parameter Unused user parameter.
 */
static void led_demo_timeout(void *parameter)
{
    static int counter = 0;
    counter++;

    /* 1. Business Logic: Cycle through Red -> Green -> Blue */
    app_rgb.r = (counter % 3 == 0);
    app_rgb.g = (counter % 3 == 1);
    app_rgb.b = (counter % 3 == 2);

    /* 2. Permission Check: Is control overridden by UDS?
     *    We query the IO service to see if the specific DID is under ShortTermAdjustment.
     */
    if (!uds_io_is_did_overridden(&led_io_service, UDS_EXAMPLE_LED_CTRL_DID))
    {
        /* Not overridden: App has control */
        hw_write_leds(app_rgb);
    }
}

/* ==========================================================================
 * UDS Service Callbacks (0x2F IO Control)
 * ========================================================================== */

/**
 * @brief Handler for RGB LED IO Control (0x2F).
 * @param did       The Data Identifier being accessed.
 * @param action    The InputOutputControlParameter (IOCP) type (e.g., ShortTermAdj).
 * @param input     Pointer to the input data buffer (ControlState + Mask).
 * @param input_len Length of the input data.
 * @param out_buf   Pointer to the output data buffer (for response).
 * @param out_len   Pointer to the length of the output buffer (in/out).
 * @return UDSErr_t UDS_PositiveResponse on success, or NRC on failure.
 */
static UDSErr_t handle_rgb_led_io(uint16_t did,
                                  uds_io_action_t action,
                                  const void *input,
                                  size_t input_len,
                                  void *out_buf,
                                  size_t *out_len)
{
    const rgb_color_t *in_val = (const rgb_color_t *)input;
    rgb_color_t *out_val = (rgb_color_t *)out_buf;

    /* Verify response buffer capacity */
    if (*out_len < sizeof(rgb_color_t))
    {
        return UDS_NRC_ResponseTooLong;
    }

    switch (action)
    {
    case UDS_IO_ACT_SHORT_TERM_ADJ: /* 0x03: UDS takes control */
        if (input_len < sizeof(rgb_color_t))
        {
            return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        }

        LOG_I("IO 0x2F: Force Set RGB [%d %d %d]", in_val->r, in_val->g, in_val->b);
        /* Immediately write the requested value to hardware */
        hw_write_leds(*in_val);
        break;

    case UDS_IO_ACT_RETURN_CONTROL: /* 0x00: Return control to ECU */
        LOG_I("IO 0x2F: Return Control to App");
        /* Immediately restore the value calculated by the App */
        hw_write_leds(app_rgb);
        break;

    case UDS_IO_ACT_FREEZE_CURRENT: /* 0x02: Freeze state */
        LOG_I("IO 0x2F: Freeze Current State");
        /* Do not touch hardware. The framework marks DID as overridden,
         * preventing App from updating it. Hardware keeps last state. */
        break;

    case UDS_IO_ACT_RESET_TO_DEFAULT: /* 0x01: Reset to default */
    {
        rgb_color_t default_off = { 0, 0, 0 };
        LOG_I("IO 0x2F: Reset to Default (OFF)");
        hw_write_leds(default_off);
        break;
    }

    default:
        return UDS_NRC_RequestOutOfRange;
    }

    /* Fill response data (Current State) */
    if (out_val)
    {
        *out_val = act_rgb;
    }
    *out_len = sizeof(rgb_color_t);

    return UDS_PositiveResponse;
}

/* Register the IO Node structure for the LED DID */
RTT_UDS_IO_NODE_DEFINE(led_io_node, UDS_EXAMPLE_LED_CTRL_DID, handle_rgb_led_io);

#endif /* UDS_ENABLE_0X2F_IO_SVC */

/* ==========================================================================
 * Integration Glue Code (CAN & System)
 * ========================================================================== */

/* 
// @brief Application CAN Transmission Thread Entry.
// @details This thread simulates normal application traffic and Network Management (NM)
//          traffic. It checks the UDS Communication Control (0x28) state before sending.
// @param parameter Unused.
static void app_can_tx_thread_entry(void *parameter)
{
    struct rt_can_msg msg = {0};

    // Initialize common CAN message properties
    msg.ide = RT_CAN_STDID; // Standard ID
    msg.rtr = RT_CAN_DTR;   // Data Frame
    msg.len = 8;            // Data Length
    
    // Dummy payload
    msg.data[0] = 0xAA;
    msg.data[1] = 0x55;

    while (1)
    {
        if (uds_env)
        {
            //Scenario 1: Normal Application Message (ID 0x123)
            //Controlled by 'commState_Normal' via Service 0x28 
            if (rtt_uds_is_app_tx_enabled(uds_env)) 
            {
                msg.id = 0x123;
                rt_device_write(can_dev, 0, &msg, sizeof(msg));
            }

            // Scenario 2: Network Management Message (ID 0x500)
            // Controlled by 'commState_NM' via Service 0x28
            if (rtt_uds_is_nm_tx_enabled(uds_env)) 
            {
                msg.id = 0x500;
                rt_device_write(can_dev, 0, &msg, sizeof(msg));
            }
        }
        rt_thread_mdelay(1000);
    }
}
*/

/**
 * @brief  User-defined CAN RX Callback.
 * @details Intercepts CAN frames from the hardware driver.
 *          1. Feeds diagnostic frames (UDS_ISO_CAN_ID_PHYS/UDS_ISO_CAN_ID_FUNC) to the UDS stack.
 *          2. Filters application frames based on UDS Communication Control (0x28).
 *
 * @param  dev  CAN device handle.
 * @param  size Number of messages available (unused in standard RTT CAN driver).
 * @return RT_EOK on success.
 */
static rt_err_t user_can_rx_callback(rt_device_t dev, rt_size_t size)
{
    struct rt_can_msg msg;

    /* Read from any hardware filter bank */
    msg.hdr_index = -1;

    if (rt_device_read(dev, 0, &msg, sizeof(msg)) == sizeof(msg))
    {
        if (uds_env)
        {
            /* 1. UDS Diagnostic Frames: MUST always pass through to the stack */
            /* Note: Adjust IDs based on your specific addressing scheme */
            if (msg.id == UDS_ISO_CAN_ID_PHYS || msg.id == UDS_ISO_CAN_ID_FUNC)
            {
                /* Non-blocking call, safe for ISR context if queue isn't full */
                rtt_uds_feed_can_frame(uds_env, &msg);
            }
            /* 2. Application Frames: Subject to Service 0x28 Control */
            else
            {
                if (rtt_uds_is_app_rx_enabled(uds_env))
                {
                    /* Forward to application message queue */
                    // rt_mq_send(app_mq, &msg, sizeof(msg));
                }
                /* else: Drop frame (Communication Control disabled RX) */
            }
        }
    }
    return RT_EOK;
}

/**
 * @brief  Initialize the default configuration for the UDS instance.
 * @param  cfg       Pointer to the configuration structure.
 * @param  dev_name  Name of the CAN device.
 */
static void uds_example_init_config(rtt_uds_config_t *cfg, const char *dev_name)
{
    cfg->can_name = dev_name;
    cfg->phys_id = UDS_ISO_CAN_ID_PHYS;
    cfg->func_id = UDS_ISO_CAN_ID_FUNC;
    cfg->resp_id = UDS_ISO_CAN_ID_RESP;
    cfg->func_resp_id = UDS_TP_NOOP_ADDR; /* No Functional Response ID required */
    cfg->func_resp_id = UDS_TP_NOOP_ADDR;

    cfg->thread_name = "uds_srv";
    cfg->stack_size = UDS_THREAD_STACK_SIZE;
    cfg->priority = UDS_THREAD_PRIORITY;
    cfg->rx_mq_pool_size = UDS_MSG_QUEUE_SIZE;
}

/**
 * @brief  Main entry point for the UDS example (MSH command).
 * @usage  uds_example [start|stop] [dev_name]
 */
static int uds_example(int argc, char **argv)
{
    rt_device_t can_dev;
    const char *cmd;
    const char *dev_name;
    rt_bool_t is_running = RT_TRUE;

    if (argc < 3)
    {
        rt_kprintf("Usage: uds_example [start|stop] [dev_name]\n");
        return 0;
    }

    cmd = argv[1];
    dev_name = argv[2];

    /* --- START Command --- */
    if (!rt_strcmp(cmd, "start"))
    {
        if (uds_env)
        {
            rt_kprintf("Error: UDS instance is already running.\n");
            return 0;
        }

        /* 1. Initialize Hardware */
        can_dev = rt_device_find(dev_name);
        if (!can_dev)
        {
            rt_kprintf("Error: CAN device '%s' not found.\n", dev_name);
            return -1;
        }

        /* Save old callback to restore on stop */
        old_can_rx_indicate = can_dev->rx_indicate;

        /* Re-configure device: Close -> Set Callback -> Open */
        rt_device_close(can_dev);
        rt_device_set_rx_indicate(can_dev, user_can_rx_callback);
        if (rt_device_open(can_dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX) != RT_EOK)
        {
            rt_kprintf("Error: Failed to open CAN device.\n");
            return -1;
        }

        /* Initialize GPIOs for LEDs */
#if (UDS_EXAMPLE_PIN_LED_R != -1)
        rt_pin_mode(UDS_EXAMPLE_PIN_LED_R, PIN_MODE_OUTPUT);
#endif
#if (UDS_EXAMPLE_PIN_LED_G != -1)
        rt_pin_mode(UDS_EXAMPLE_PIN_LED_G, PIN_MODE_OUTPUT);
#endif
#if (UDS_EXAMPLE_PIN_LED_B != -1)
        rt_pin_mode(UDS_EXAMPLE_PIN_LED_B, PIN_MODE_OUTPUT);
#endif

        /* Configure Hardware Filters (Accept all std frames) */
#ifdef RT_CAN_USING_HDR
        struct rt_can_filter_item items[] = {
            { .id = 0, .ide = RT_CAN_STDID, .rtr = RT_CAN_DTR, .mode = RT_CAN_MODE_MASK, .mask = 0, .hdr_bank = -1 },
        };
        struct rt_can_filter_config filter_cfg = { .count = 1, .actived = 1, .items = items };
        rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &filter_cfg);
#endif
        /* Start CAN device */
        rt_device_control(can_dev, RT_CAN_CMD_START, &is_running);

        /* 2. Prepare UDS Configuration */
        rtt_uds_config_t cfg;
        uds_example_init_config(&cfg, dev_name);

        /* 3. Create UDS Library Instance */
        uds_env = rtt_uds_create(&cfg);
        if (!uds_env)
        {
            rt_kprintf("Error: Failed to create UDS instance (Out of memory?).\n");
            /* 1. Stop CAN device */
            is_running = RT_FALSE; 
            rt_device_control(can_dev, RT_CAN_CMD_START, &is_running);

            /* 2. Restore original callback */
            rt_device_set_rx_indicate(can_dev, old_can_rx_indicate);

            /* 3. Close device */
            rt_device_close(can_dev);
            
            /* 4. Reset global handle */
            can_dev = RT_NULL;
            return -1;
        }

        /* 4. Register Services */
        log_timeout_node_register(uds_env);

#ifdef UDS_ENABLE_SESSION_SVC
        session_control_node_register(uds_env);
#endif // UDS_ENABLE_SESSION_SVC

#ifdef UDS_ENABLE_SECURITY_SVC
        rtt_uds_sec_service_mount(uds_env, &security_service);
#endif // UDS_ENABLE_SECURITY_SVC

#ifdef UDS_ENABLE_PARAM_SVC
        param_rdbi_node_register(uds_env);
        param_wdbi_node_register(uds_env);
#endif // UDS_ENABLE_PARAM_SVC

#ifdef UDS_ENABLE_CONSOLE_SVC
        rtt_uds_console_service_mount(uds_env, &console_service);
#endif // UDS_ENABLE_CONSOLE_SVC

#ifdef UDS_ENABLE_FILE_SVC
        rtt_uds_file_service_mount(uds_env, &file_service);
#endif // UDS_ENABLE_FILE_SVC

#ifdef UDS_ENABLE_0X2F_IO_SVC
        /* 4.1 Register the node implementation to the service definition */
        uds_io_register_node(&led_io_service, &led_io_node);

        /* 4.2 Mount the service to the UDS environment */
        rtt_uds_io_service_mount(uds_env, &led_io_service);

        /* 4.3 Start the application timer */
        led_timer = rt_timer_create("uds_exled", led_demo_timeout, RT_NULL,
                                    500, /* 500ms period */
                                    RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);
        if (led_timer)
            rt_timer_start(led_timer);
#endif // UDS_ENABLE_0X2F_IO_SVC

#ifdef UDS_ENABLE_0X11_RESET_SVC
        reset_req_node_register(uds_env);
        reset_exec_node_register(uds_env);
#endif // UDS_ENABLE_0X11_RESET_SVC

#ifdef UDS_ENABLE_0X28_COMM_CTRL_SVC
        rtt_uds_comm_ctrl_service_mount(uds_env, &comm_ctrl_service);
#endif //UDS_ENABLE_0X28_COMM_CTRL_SVC

        rt_kprintf("UDS Server started on %s.\n", dev_name);
    }
    /* --- STOP Command --- */
    else if (!rt_strcmp(cmd, "stop"))
    {
        if (uds_env)
        {
            rt_kprintf("Stopping UDS Server...\n");

#ifdef UDS_ENABLE_0X2F_IO_SVC
            if (led_timer)
            {
                rt_timer_stop(led_timer);
                rt_timer_delete(led_timer);
                led_timer = RT_NULL;
            }
            /* Optional: Unregister node */
            uds_io_unregister_node(&led_io_service, &led_io_node);
#endif // UDS_ENABLE_0X2F_IO_SVC

#ifdef UDS_ENABLE_CONSOLE_SVC
            rtt_uds_console_service_unmount(&console_service);
#endif // UDS_ENABLE_CONSOLE_SVC

            /* 1. Unregister all services from environment */
            rtt_uds_service_unregister_all(uds_env);

            /* 2. Destroy UDS Environment */
            rtt_uds_destroy(uds_env);
            uds_env = RT_NULL;

            /* 3. Restore Hardware Configuration */
            can_dev = rt_device_find(dev_name);
            if (can_dev)
            {
                /* Restore the original callback (e.g., from CAN Open protocol stack or default) */
                rt_device_set_rx_indicate(can_dev, old_can_rx_indicate);
                rt_device_close(can_dev);
            }
            rt_kprintf("UDS Server stopped.\n");
        }
        else
        {
            rt_kprintf("Warning: UDS is not running.\n");
        }
    }
    else
    {
        rt_kprintf("Invalid command: %s\n", cmd);
    }

    return 0;
}
/* Register command to MSH (Melon Shell) */
MSH_CMD_EXPORT(uds_example, Start / Stop UDS server example);

/**
 * @brief  MSH Command: List all registered UDS services.
 * @details Useful for debugging to verify which SIDs are active.
 */
static void uds_list(void)
{
    if (uds_env)
    {
        rtt_uds_dump_services(uds_env);
    }
    else
    {
        rt_kprintf("Error: UDS Server is not running.\n");
    }
}
MSH_CMD_EXPORT(uds_list, List registered UDS services);

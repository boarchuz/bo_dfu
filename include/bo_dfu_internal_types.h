#ifndef BO_DFU_INTERNAL_TYPES_H
#define BO_DFU_INTERNAL_TYPES_H

#include <assert.h>
#include <stdint.h>

#include "bo_dfu_usb.h"
#include "bo_dfu_ota.h"

typedef enum {
    BO_DFU_BUS_RESET = -3,
    BO_DFU_BUS_DESYNCED = -2,
    BO_DFU_BUS_SYNCED = -1,
    BO_DFU_BUS_OK = 0,
} bo_dfu_bus_state_t;

typedef enum {
    BO_DFU_FSM_IDLE = 0,
    BO_DFU_FSM_DNLOAD_IDLE,
    BO_DFU_FSM_DNLOAD_SYNC_READY,
    BO_DFU_FSM_MANIFEST_SYNC_READY,
    BO_DFU_FSM_DNLOAD_SYNC_DONE,
    BO_DFU_FSM_ERROR,
    BO_DFU_FSM_MANIFEST_SYNC_DONE,
    BO_DFU_FSM_COMPLETE,

    BO_DFU_FSM_MAX,
    BO_DFU_FSM_MINIMUM_COMPLETE_VAL = BO_DFU_FSM_MANIFEST_SYNC_DONE,
} bo_dfu_fsm_t;
#define BO_DFU_FSM(state) BO_DFU_FSM_ ## state
#define BO_DFU_FSM_IS_COMPLETE(state) ((state) >= BO_DFU_FSM_MINIMUM_COMPLETE_VAL)

_Static_assert(
    BO_DFU_FSM_MANIFEST_SYNC_DONE  >= BO_DFU_FSM_MINIMUM_COMPLETE_VAL &&
    BO_DFU_FSM_COMPLETE            >= BO_DFU_FSM_MINIMUM_COMPLETE_VAL, ""
);

typedef enum {
    #define BO_DFU_SET_FSM_ENUM_VAL(protocol, next, fsm) ((((protocol) & 0xFF) << 16) | (((next) & 0xFF) << 0) | (((fsm) & 0xFF) << 24))
        BO_DFU_SET_FSM_IDLE                   = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_dfuIDLE,                 BO_DFU_STATE_PROTOCOL_dfuIDLE,                 BO_DFU_FSM_IDLE),
        BO_DFU_SET_FSM_DNLOAD_SYNC_READY      = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_dfuDNLOAD_SYNC,          BO_DFU_STATE_PROTOCOL_dfuDNBUSY,               BO_DFU_FSM_DNLOAD_SYNC_READY),
        BO_DFU_SET_FSM_DNLOAD_SYNC_DONE       = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_dfuDNLOAD_SYNC,          BO_DFU_STATE_PROTOCOL_dfuDNLOAD_IDLE,          BO_DFU_FSM_DNLOAD_SYNC_DONE),
        BO_DFU_SET_FSM_DNLOAD_IDLE            = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_dfuDNLOAD_IDLE,          BO_DFU_STATE_PROTOCOL_dfuDNLOAD_IDLE,          BO_DFU_FSM_DNLOAD_IDLE),
        BO_DFU_SET_FSM_MANIFEST_SYNC_READY    = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_dfuMANIFEST_SYNC,        BO_DFU_STATE_PROTOCOL_dfuMANIFEST,             BO_DFU_FSM_MANIFEST_SYNC_READY),
        BO_DFU_SET_FSM_MANIFEST_SYNC_DONE     = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_dfuMANIFEST_SYNC,        BO_DFU_STATE_PROTOCOL_appIDLE,                 BO_DFU_FSM_MANIFEST_SYNC_DONE),
        BO_DFU_SET_FSM_ERROR                  = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_dfuERROR,                BO_DFU_STATE_PROTOCOL_dfuERROR,                BO_DFU_FSM_ERROR),
        BO_DFU_SET_FSM_COMPLETE               = BO_DFU_SET_FSM_ENUM_VAL(BO_DFU_STATE_PROTOCOL_appIDLE,                 BO_DFU_STATE_PROTOCOL_appIDLE,                 BO_DFU_FSM_COMPLETE),
    #undef BO_DFU_SET_FSM_ENUM_VAL
} bo_dfu_set_fsm_t;
#define BO_DFU_SET_FSM(state) BO_DFU_SET_FSM_ ## state

typedef struct {
    union {
        uint32_t bmRequestType_and_bRequest;
        uint32_t request_is_active;
    };
    uint32_t wValue;
    union {
        const void *direction_is_device_to_host;
        const uint8_t *data;
    };
    size_t len;
    size_t counter;
} bo_dfu_usb_transfer_t;

typedef struct {
    esp_bl_usb_ota_partition_t ota;
    bo_dfu_bus_state_t state;
    // Everything below here is cleared upon bus reset
    #define BO_DFU_T_TO_BUS_RESET_PTR(x) (&((x)->state) + 1)
    #define BO_DFU_T_BUS_RESET_SIZE (sizeof(bo_dfu_t) - (offsetof(bo_dfu_t, state) + sizeof(((bo_dfu_t*)0)->state)))
    union {
        struct {
            uint8_t address;
            uint8_t configuration_value;
            // These two bytes will always be zero so may as well use when needing to send 0s to the host.
            union {
                uint16_t get_status_response_buffer;
                uint8_t get_interface_response_buffer;
            };
        };
        uint32_t address_and_configuration;
    };
    bo_dfu_usb_transfer_t transfer;
    #define BO_DFU_IS_ADDRESSED(x) (((x)->address_and_configuration) > 0)
    #define BO_DFU_IS_CONFIGURED(x) (((x)->configuration_value) > 0)
    struct {
        union {
            bo_dfu_get_status_response_t status_response;
            struct {
                union {
                    struct {
                        uint32_t status : 8;
                        uint32_t /* bwPollTimeout */ : 24;
                    };
                    uint32_t status_and_poll_timeout;
                    #define BO_DFU_STATUS_AND_POLL_TIMEOUT32(status, poll_timeout) (((status) << 0) | ((poll_timeout) << 8))
                };
                union {
                    struct {
                        union {
                            uint8_t next_state;
                            // Given that this byte is set to appIDLE (0) on completion, it doubles as a boolean representing incompletion.
                            uint8_t is_incomplete;
                        };
                        uint8_t /* iString */ : 8;
                        uint8_t get_state_response;
                        uint8_t state_get;
                    };
                    uint32_t state_set;
                };
            };
        };
        union {
            uint32_t block_num;
            struct {
                uint32_t block_num_counter : 31;
                uint32_t block_num_final : 1;
            };
        };
        union {
            uint8_t buffer[0x1000];
            uint32_t buffer_aligned[0x1000 / sizeof(uint32_t)]; // this must be 32b aligned for flash_write purposes
        };
    } dfu;
} bo_dfu_t;
#define BO_DFU_T_GET_STATE(x) ((x)->dfu.state_get)
#define BO_DFU_T_IS_COMPLETE(x) BO_DFU_FSM_IS_COMPLETE(BO_DFU_T_GET_STATE(x))

#endif /* BO_DFU_INTERNAL_TYPES_H */

#ifndef BO_DFU_USB_RX_H
#define BO_DFU_USB_RX_H

#include <assert.h>
#include <stdint.h>

#include "soc/gpio_reg.h"

#include "bo_dfu_internal.h"
#include "bo_dfu_gpio.h"
#include "bo_dfu_time.h"

typedef struct {
    union {
        struct {
            uint8_t bmRequestType;
            uint8_t bRequest;
        };
        uint16_t bmRequestType_and_bRequest;
    };
    union {
        struct {
            uint8_t index;
            uint8_t type;
        } get_descriptor;
        uint16_t wValue;
    };
    union {
        struct {
            uint16_t wIndex;
            uint16_t wLength;
        };
        // This is unused but ensures alignment for optimisation purposes.
        uint32_t wIndex_and_wLength;
    };
} bo_dfu_usb_rx_setup_data_t;

_Static_assert(sizeof(bo_dfu_usb_rx_setup_data_t) == (sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t)), "");

_Static_assert(offsetof(bo_dfu_usb_rx_setup_data_t, bmRequestType_and_bRequest) == 0, "");
_Static_assert(offsetof(bo_dfu_usb_rx_setup_data_t, wValue) == 2, "");
_Static_assert(offsetof(bo_dfu_usb_rx_setup_data_t, wIndex) == 4, "");
_Static_assert(offsetof(bo_dfu_usb_rx_setup_data_t, wIndex_and_wLength) == 4, "");
_Static_assert(offsetof(bo_dfu_usb_rx_setup_data_t, wLength) == 6, "");


// Adjust alignment of the packet buffer to enable some optimisations.
#define BO_DFU_RX_PACKET_PADDING_SIZE 2

typedef struct {
    union {
        struct {
            uint8_t padding1[BO_DFU_RX_PACKET_PADDING_SIZE];
            union {
                struct {
                    uint8_t sync;
                    union {
                        struct {
                            uint8_t pid_check4 : 4;
                            uint8_t pid4 : 4;
                        };
                        uint8_t pid_with_check;
                    };
                };
                uint16_t sync_and_pid_with_check;
            };
            union {
                union {
                    struct {
                        uint16_t address : 7;
                        uint16_t endpoint : 4;
                        uint16_t crc : 5;
                    };
                    uint8_t token_bytes[2];
                    uint16_t token;
                };
                union {
                    uint8_t data[8];
                    bo_dfu_usb_rx_setup_data_t setup_data;
                };
            };
        };
        struct {
            uint8_t padding2[BO_DFU_RX_PACKET_PADDING_SIZE];
            uint8_t buffer[/* sync */ 1 + /* pid */ 1 + /* data */ 8 + /* crc */ 2];
        };
    };
} bo_dfu_usb_rx_packet_t;

_Static_assert(
    offsetof(bo_dfu_usb_rx_packet_t, sync) == BO_DFU_RX_PACKET_PADDING_SIZE + 0 &&
    offsetof(bo_dfu_usb_rx_packet_t, pid_with_check) == BO_DFU_RX_PACKET_PADDING_SIZE + 1 &&
    offsetof(bo_dfu_usb_rx_packet_t, token) == BO_DFU_RX_PACKET_PADDING_SIZE + 2, ""
);

_Static_assert(
    offsetof(bo_dfu_usb_rx_packet_t, setup_data) == BO_DFU_RX_PACKET_PADDING_SIZE + 2, ""
);

_Static_assert(sizeof(((bo_dfu_usb_rx_packet_t*)0)->buffer) == 12, "");

#define BO_DFU_USB_BUS_RAW_TO_RX(bus) (((bus) >> BO_DFU_GPIO_SHIFT) & (BO_DFU_GPIO_MASK >> BO_DFU_GPIO_SHIFT))

typedef enum {
    BO_DFU_USB_RX_BUS_SE0      = BO_DFU_USB_BUS_RAW_TO_RX(BO_DFU_BUS_SE0),
    BO_DFU_USB_RX_BUS_J        = BO_DFU_USB_BUS_RAW_TO_RX(BO_DFU_BUS_J),
    BO_DFU_USB_RX_BUS_K        = BO_DFU_USB_BUS_RAW_TO_RX(BO_DFU_BUS_K),
    BO_DFU_USB_RX_BUS_INVALID  = BO_DFU_USB_BUS_RAW_TO_RX(BO_DFU_BUS_SE1),
} bo_dfu_usb_rx_bus_state_t;

static IRAM_ATTR bo_dfu_usb_rx_bus_state_t bo_dfu_usb_rx_bus_state(void)
{
    return BO_DFU_USB_BUS_RAW_TO_RX(REG_READ(BO_DFU_GPIO_REG_IN));
}

static IRAM_ATTR int bo_dfu_usb_rx_byte(uint32_t *bit_time, uint32_t *previous_bus, int *consecutive)
{
    uint8_t byte = 0;
    for(int bit = 0; bit < 8 || *consecutive == 6 /* <-- ensures wait/check for stuffing bit before ending byte */;)
    {
        uint32_t new_bus = bo_dfu_usb_rx_bus_state();
        // Waits until the time of the next transition here...
        while(bo_dfu_ccount() - *bit_time < (BO_DFU_USB_CPU_CYCLES_PER_BIT /* + BO_DFU_USB_CPU_CYCLES_WAIT_READ */));
        // ... then processes last bus state. The following processing time doubles as a brief delay to allow the bus to settle:
        switch(new_bus)
        {
            case BO_DFU_USB_RX_BUS_J:
            case BO_DFU_USB_RX_BUS_K:
                uint32_t toggled = new_bus ^ *previous_bus;
                *previous_bus = new_bus;
                if(toggled)
                {
                    if(*consecutive < 6)
                    {
                        byte >>= 1;
                        ++bit;
                    }
                    *consecutive = 0;
                }
                else
                {
                    if(++*consecutive >= 7) return BO_DFU_BUS_DESYNCED;
                    byte >>= 1;
                    byte |= (1 << 7);
                    ++bit;
                }
                break;
            case BO_DFU_USB_RX_BUS_SE0:
                if(bit == 0)
                {
                    return BO_DFU_BUS_SYNCED;
                }
                // falls through
            default:
                return BO_DFU_BUS_DESYNCED;
        }
        *bit_time += BO_DFU_USB_CPU_CYCLES_PER_BIT;
    }
    _Static_assert(
        BO_DFU_BUS_RESET < 0 &&
        BO_DFU_BUS_DESYNCED < 0 &&
        BO_DFU_BUS_SYNCED < 0, ""
    );
    return byte;
}

static IRAM_ATTR int bo_dfu_usb_rx_packet_buffer(uint32_t *bit_time, bo_dfu_usb_rx_packet_t *packet)
{
    int consecutive = 0;
    uint32_t previous_bus = BO_DFU_USB_RX_BUS_J;
    int received = 0;
    #if 0
        // We reach here upon bus transition (or probing for EOP).
        // In practice, there's enough processing time to not require an explicit delay here before reading the new bus state.
        while(bo_dfu_ccount() - *bit_time < BO_DFU_USB_CPU_CYCLES_WAIT_READ);
    #endif
    for(; received < sizeof(packet->buffer); ++received)
    {
        int byte = bo_dfu_usb_rx_byte(bit_time, &previous_bus, &consecutive);
        if(byte < 0)
        {
            if(byte == BO_DFU_BUS_SYNCED)
            {
                return received;
            }
            return BO_DFU_BUS_DESYNCED;
        }
        packet->buffer[received] = byte;
    }
    return received;
}

static IRAM_ATTR bool bo_dfu_usb_rx_wait_transition(uint32_t bus, uint32_t *bit_time, uint32_t timeout)
{
    for(;;)
    {
        uint32_t now = bo_dfu_ccount();
        if(now - *bit_time > timeout)
        {
            return false;
        }
        if(bo_dfu_usb_rx_bus_state() != bus)
        {
            *bit_time = now;
            return true;
        }
    }
}

static IRAM_ATTR bool bo_dfu_usb_bus_rx_check_reset(uint32_t *bit_time)
{
    return !bo_dfu_usb_rx_wait_transition(BO_DFU_USB_RX_BUS_SE0, bit_time, BO_DFU_USB_RESET_SIGNAL_CYCLES);
}

static IRAM_ATTR bo_dfu_bus_state_t bo_dfu_usb_bus_rx_check_eop(uint32_t *bit_time)
{
    // Check EOP (note: not necessarily SE0 here so must fail successfully if not)
    uint32_t se0_start_time = *bit_time;

    if(bo_dfu_usb_bus_rx_check_reset(bit_time))
    {
        return BO_DFU_BUS_RESET;
    }
    while(bo_dfu_ccount() - *bit_time < BO_DFU_USB_CPU_CYCLES_WAIT_READ);
    if(bo_dfu_usb_rx_bus_state() != BO_DFU_USB_RX_BUS_J)
    {
        return BO_DFU_BUS_DESYNCED;
    }
    if(*bit_time - se0_start_time < BO_DFU_USB_CPU_CYCLES_PER_BIT)
    {
        return BO_DFU_BUS_SYNCED;
    }
    // Else valid EOP
    return BO_DFU_BUS_OK;
}

static IRAM_ATTR int bo_dfu_usb_rx_buffer_and_check_eop(uint32_t *bit_time, bo_dfu_usb_rx_packet_t *packet)
{
    int bytes_received = bo_dfu_usb_rx_packet_buffer(bit_time, packet);
    if(bytes_received < 0)
    {
        return bytes_received;
    }
    int eop_check = bo_dfu_usb_bus_rx_check_eop(bit_time);
    if(eop_check < 0)
    {
        return eop_check;
    }
    return bytes_received;
}

static IRAM_ATTR bool bo_dfu_usb_rx_wait_sop(uint32_t *bit_time, uint32_t timeout)
{
    return bo_dfu_usb_rx_wait_transition(BO_DFU_USB_RX_BUS_J, bit_time, timeout);
}

static IRAM_ATTR int bo_dfu_usb_rx_next_packet(uint32_t *bit_time, bo_dfu_usb_rx_packet_t *packet)
{
    /**
     * Timeout is necessary when waiting for ACK or followup data to SETUP/OUT token.
     * It also ensures control is returned to the parent loop at regular intervals when the bus is disconnected/idle.
     * This occurs quickly enough that a SOP should not be missed, even if timed unfortunately.
    */
    uint32_t timeout = (BO_DFU_USB_WAIT_DATA_TIMEOUT_BIT_TIMES * BO_DFU_USB_CPU_CYCLES_PER_BIT);
    if(!bo_dfu_usb_rx_wait_sop(bit_time, timeout))
    {
        return BO_DFU_BUS_SYNCED;
    }
    return bo_dfu_usb_rx_buffer_and_check_eop(bit_time, packet);
}

#endif /* BO_DFU_USB_RX_H */

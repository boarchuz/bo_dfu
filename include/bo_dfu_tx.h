#ifndef BO_DFU_USB_TX_H
#define BO_DFU_USB_TX_H

#include <assert.h>
#include <stdint.h>

#include "esp_attr.h"

#include "bo_dfu_gpio.h"
#include "bo_dfu_time.h"

#define BO_DFU_USB_RAW_TO_TX(bus) ((bus) >> BO_DFU_GPIO_SHIFT)
#define BO_DFU_USB_TX_TO_RAW(bus) ((bus) << BO_DFU_GPIO_SHIFT)

typedef enum {
    BO_DFU_USB_TX_SE0  = BO_DFU_USB_RAW_TO_TX(BO_DFU_BUS_SE0),
    BO_DFU_USB_TX_J    = BO_DFU_USB_RAW_TO_TX(BO_DFU_BUS_J),
    BO_DFU_USB_TX_K    = BO_DFU_USB_RAW_TO_TX(BO_DFU_BUS_K),
} bo_dfu_usb_tx_bus_state_t;

_Static_assert((uint32_t)BO_DFU_USB_TX_TO_RAW(BO_DFU_USB_TX_J) == (uint32_t)BO_DFU_BUS_J, "");
_Static_assert((uint32_t)BO_DFU_USB_TX_TO_RAW(BO_DFU_USB_TX_K) == (uint32_t)BO_DFU_BUS_K, "");
_Static_assert((uint32_t)BO_DFU_USB_TX_TO_RAW(BO_DFU_USB_TX_SE0) == (uint32_t)BO_DFU_BUS_SE0, "");

static IRAM_ATTR void bo_dfu_usb_tx_bus_state(bo_dfu_usb_tx_bus_state_t signal)
{
    uint32_t reg = REG_READ(BO_DFU_GPIO_REG_OUT);
    reg &= ~BO_DFU_GPIO_MASK;
    reg |= BO_DFU_USB_TX_TO_RAW(signal);
    REG_WRITE(BO_DFU_GPIO_REG_OUT, reg);
}

static IRAM_ATTR void bo_dfu_usb_tx_enable(void)
{
    REG_WRITE(BO_DFU_GPIO_REG_OUT_EN, BO_DFU_GPIO_MASK);
}

static IRAM_ATTR void bo_dfu_usb_tx_disable(void)
{
    REG_WRITE(BO_DFU_GPIO_REG_OUT_DIS, BO_DFU_GPIO_MASK);
}

static IRAM_ATTR void bo_dfu_usb_tx_bus_states(uint32_t *bit_time, const bo_dfu_usb_tx_bus_state_t *signals, size_t signal_len)
{
    for(size_t i = 0; i < signal_len; ++i)
    {
        while(bo_dfu_ccount() - *bit_time < BO_DFU_USB_CPU_CYCLES_PER_BIT);
        *bit_time += BO_DFU_USB_CPU_CYCLES_PER_BIT;
        bo_dfu_usb_tx_bus_state(signals[i]);
    }
}

static IRAM_ATTR void bo_dfu_usb_tx_eop(uint32_t *bit_time)
{
    bo_dfu_usb_tx_bus_state_t signals[] = {BO_DFU_USB_TX_SE0, BO_DFU_USB_TX_SE0, BO_DFU_USB_TX_J};
    bo_dfu_usb_tx_bus_states(bit_time, signals, ARRAY_SIZE(signals));
    bo_dfu_usb_tx_disable();
}

static IRAM_ATTR void bo_dfu_usb_tx_buffer(uint32_t *bit_time, const uint8_t *data, size_t len)
{
    bo_dfu_usb_tx_bus_state_t signal = BO_DFU_USB_TX_J;
    int consecutive = 0;

    for(size_t i = 0; i < len; ++i)
    {
        uint32_t this_byte = data[i];
        for(int b = 0; b < 8;)
        {
            if((this_byte & 1) == 0)
            {
                signal ^= (BO_DFU_USB_TX_J | BO_DFU_USB_TX_K);
                consecutive = 0;
            }
            else
            {
                ++consecutive;
            }
            bo_dfu_usb_tx_bus_states(bit_time, &signal, 1);
            if(consecutive >= 6)
            {
                this_byte &= ~1;
            }
            else
            {
                this_byte >>= 1;
                ++b;
            }
        }
    }
}

static IRAM_ATTR void bo_dfu_usb_tx_init(uint32_t *bit_time)
{
    #if 1
        // There is always just enough overhead that it's not strictly necessary to add a delay between receiving a packet and starting a transmission.
        // However, the ack does begin very soon after a null data packet, so a little extra delay is beneficial.
        // Most importantly, this initialises D+/D- GPIOs to BO_DFU_USB_TX_J on the very first transmission.
        bo_dfu_usb_tx_bus_state_t signals[] = {BO_DFU_USB_TX_J,};
        bo_dfu_usb_tx_bus_states(bit_time, signals, ARRAY_SIZE(signals));
    #endif
    bo_dfu_usb_tx_enable();
}

static IRAM_ATTR uint32_t bo_dfu_usb_tx_packet(const void *packet, size_t len)
{
    uint32_t bit_time = bo_dfu_ccount();
    bo_dfu_usb_tx_init(&bit_time);
    bo_dfu_usb_tx_buffer(&bit_time, (const uint8_t*)packet, len);
    bo_dfu_usb_tx_eop(&bit_time);
    return bit_time;
}

static IRAM_ATTR uint32_t bo_dfu_usb_tx_data(const uint8_t pid_with_check, const uint8_t *data, size_t data_len)
{
    union {
        struct {
            uint8_t sync;
            uint8_t pid;
            union {
                uint8_t data[8];
                uint8_t data_and_crc[8 + sizeof(uint16_t)];
            };
        };
    } packet;
    packet.sync = BO_DFU_USB_SYNC_BYTE;
    packet.pid = pid_with_check;
    size_t tx_len = sizeof(uint8_t) + sizeof(uint8_t);
    if(data_len > 0)
    {
        memcpy(packet.data, data, data_len);
        tx_len += data_len;
    }
    // Conveniently, the PID LSB correlates with the need to append a CRC for the only 4 PIDs transmitted in this application.
    #define USB_PID_HAS_DATA(pid) ((pid) & 1)
    // Ensure this condition holds true.
    _Static_assert(
        USB_PID_HAS_DATA(BO_DFU_USB_PID_CHECK_DATA0 & 1) &&
        USB_PID_HAS_DATA(BO_DFU_USB_PID_CHECK_DATA1 & 1) &&
        !USB_PID_HAS_DATA(BO_DFU_USB_PID_CHECK_ACK & 1) &&
        !USB_PID_HAS_DATA(BO_DFU_USB_PID_CHECK_STALL & 1), ""
    );
    if(USB_PID_HAS_DATA(pid_with_check))
    {
        uint16_t *crc = (uint16_t*)&packet.data_and_crc[data_len];
        *crc = bo_dfu_crc_data(data, data_len);
        tx_len += sizeof(uint16_t);
    }
    return bo_dfu_usb_tx_packet(&packet, tx_len);
}

FORCE_INLINE_ATTR IRAM_ATTR bool bo_dfu_usb_tx_data_with_checks(const uint8_t pid_with_check, const uint8_t *data, size_t data_len)
{
    if(
        !(
            pid_with_check == BO_DFU_USB_PID_CHECK_DATA0 ||
            pid_with_check == BO_DFU_USB_PID_CHECK_DATA1 ||
            pid_with_check == BO_DFU_USB_PID_CHECK_ACK ||
            pid_with_check == BO_DFU_USB_PID_CHECK_STALL
        )
    )
    {
        asm("error");
    }
    if(data_len > BO_DFU_USB_LOW_SPEED_PACKET_SIZE)
    {
        asm("error");
    }
    return bo_dfu_usb_tx_data(pid_with_check, data, data_len);
}

FORCE_INLINE_ATTR IRAM_ATTR void bo_dfu_usb_tx_handshake(uint8_t pid_with_check)
{
    bo_dfu_usb_tx_data_with_checks(pid_with_check, NULL, 0);
}

#endif /* BO_DFU_USB_TX_H */

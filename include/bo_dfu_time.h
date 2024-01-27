#ifndef BO_DFU_TIME_H
#define BO_DFU_TIME_H

#include <stdint.h>

#include "esp_attr.h"

#if __has_include ("esp_cpu.h")
#define BO_DFU_TIME_USE_ESP_CPU 1
#include "esp_cpu.h"
#else
#include "hal/cpu_ll.h"
#endif

#include "bo_dfu_usb.h"
#include "bo_dfu_clk.h"

#define BO_DFU_USB_CPU_CYCLES_PER_BIT (BO_DFU_CPU_FREQ_MHZ * 1000 * 1000 / BO_DFU_USB_LOW_SPEED_FREQ_HZ)
#define BO_DFU_USB_CPU_CYCLES_WAIT_READ (BO_DFU_USB_CPU_CYCLES_PER_BIT / 32)
#define BO_DFU_USB_RESET_SIGNAL_CYCLES (BO_DFU_USB_RESET_SIGNAL_NS * BO_DFU_CPU_FREQ_MHZ / 1000)

static inline IRAM_ATTR uint32_t bo_dfu_ccount(void)
{
#ifdef BO_DFU_TIME_USE_ESP_CPU
    return esp_cpu_get_cycle_count();
#else
    return cpu_ll_get_cycle_count();
#endif
}

#endif /* BO_DFU_TIME_H */

#ifndef BO_DFU_CLK_H
#define BO_DFU_CLK_H

#include "esp_attr.h"
#include "soc/rtc.h"
#include "hal/efuse_hal.h"
#include "esp_rom_uart.h"

#include "bo_dfu_log.h"
#include "bo_dfu_util.h"

#include "sdkconfig.h"

#define BO_DFU_CPU_FREQ_MHZ 240

#define BO_DFU_MS_TO_CCOUNT_FREQ(freq_mhz, ms) ((ms) * (freq_mhz) * 1000)
#define BO_DFU_MS_TO_CCOUNT(ms) BO_DFU_MS_TO_CCOUNT_FREQ(BO_DFU_CPU_FREQ_MHZ, ms)

static void IRAM_ATTR bo_dfu_clock_init(void)
{
    // Increase CPU frequency to 240MHz
    #if CONFIG_BOOTLOADER_LOG_LEVEL > 0
        if(efuse_hal_get_rated_freq_mhz() < BO_DFU_CPU_FREQ_MHZ)
        {
            ESP_LOGE(BO_DFU_TAG, "[%s] not rated for required CPU frequency", __func__);
            for(;;);
        }
    #endif
    #if CONFIG_BOOTLOADER_LOG_LEVEL > 0
        esp_rom_uart_tx_wait_idle(0);
    #endif
    rtc_clk_config_t clk_cfg = RTC_CLK_CONFIG_DEFAULT();
    clk_cfg.cpu_freq_mhz = BO_DFU_CPU_FREQ_MHZ;
    rtc_clk_init(clk_cfg);
}

static void IRAM_ATTR bo_dfu_clock_deinit(void)
{
    // Return to default clock config
    #if CONFIG_BOOTLOADER_LOG_LEVEL > 0
        esp_rom_uart_tx_wait_idle(0);
    #endif
    rtc_clk_config_t clk_cfg = RTC_CLK_CONFIG_DEFAULT();
    rtc_clk_init(clk_cfg);
}

#endif /* BO_DFU_CLK_H */

#ifndef BO_DFU_H
#define BO_DFU_H

#include <stdint.h>
#include <sys/param.h>
#include <string.h>

#include "esp_rom_sys.h"

#include "bo_dfu_usb.h"
#include "bo_dfu_descriptor.h"
#include "bo_dfu_internal.h"
#include "bo_dfu_clk.h"
#include "bo_dfu_ota.h"
#include "bo_dfu_wdt.h"
#include "bo_dfu_usb.h"
#include "bo_dfu_crc.h"
#include "bo_dfu_util.h"
#include "bo_dfu_rx.h"
#include "bo_dfu_tx.h"
#include "bo_dfu_transfer.h"
#include "bo_dfu_gpio.h"
#include "bo_dfu_log.h"
#include "bo_dfu_time.h"

#include "sdkconfig.h"

static IRAM_ATTR void bo_dfu_fsm(bo_dfu_t *dfu)
{
    switch(dfu->state)
    {
        case BO_DFU_BUS_INIT:
        {
            uint32_t reset_check_time = bo_dfu_ccount();
            if(!bo_dfu_usb_bus_rx_check_reset(&reset_check_time))
            {
                break;
            }
            ESP_LOGD(BO_DFU_TAG, "[%s] initial bus reset", __func__);
            bo_dfu_update_state(dfu, IDLE, BO_DFU_STATUS_OK);
            dfu->state = BO_DFU_BUS_RESET;
        }
        /* falls through */
        case BO_DFU_BUS_RESET:
        {
            ESP_LOGD(BO_DFU_TAG, "[%s] bus reset", __func__);
            memset(BO_DFU_T_TO_BUS_RESET_PTR(dfu), 0, BO_DFU_T_BUS_RESET_SIZE);
            dfu->state = BO_DFU_BUS_DESYNCED;
        }
        /* falls through */
        case BO_DFU_BUS_DESYNCED:
        {
            // Wait for valid EOP
            uint32_t eop_check_time = bo_dfu_ccount();
            bo_dfu_bus_state_t eop_check = bo_dfu_usb_bus_rx_check_eop(&eop_check_time);
            if(eop_check != BO_DFU_BUS_OK)
            {
                dfu->state = eop_check;
                break;
            }
        }
        /* falls through */
        case BO_DFU_BUS_SYNCED:
        case BO_DFU_BUS_OK:
        {
            dfu->state = bo_dfu_usb_transaction_next(dfu);
            break;
        }
    }
}

static bool IRAM_ATTR bo_dfu_is_compatible_reset_type(void)
{
    switch(esp_rom_get_reset_reason(0))
    {
        case RESET_REASON_CHIP_POWER_ON:
        case RESET_REASON_SYS_BROWN_OUT:
        case RESET_REASON_SYS_RTC_WDT:
            return true;
        default:
            return false;
    }
}

static esp_err_t IRAM_ATTR bo_dfu_init(bo_dfu_t *dfu)
{
    memset(dfu, 0, sizeof(*dfu));
    dfu->state = BO_DFU_BUS_INIT;

    if(ESP_OK != bo_dfu_ota_init(&dfu->ota))
    {
        // No OTA partition?
        ESP_LOGE(BO_DFU_TAG, "[%s] invalid OTA configuration", __func__);
        return ESP_FAIL;
    }

    bo_dfu_descriptor_init();
    bo_dfu_clock_init();
    return ESP_OK;
}

static void IRAM_ATTR bo_dfu_deinit(void)
{
    bo_dfu_clock_deinit();
}

#endif /* BO_DFU_H */

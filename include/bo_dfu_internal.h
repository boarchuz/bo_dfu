#ifndef BO_DFU_INTERNAL_H
#define BO_DFU_INTERNAL_H

#include <stdint.h>

#if __has_include("esp_app_desc.h")
#   include "esp_app_desc.h"
#endif
#include "esp_image_format.h"

#include "bo_dfu_log.h"

#include "bo_dfu_internal_types.h"

// For 0x1000 blocks, takes about 45ms to erase and write.
#define BO_DFU_DOWNLOAD_SYNC_POLL_TIMEOUT_MS 250
// ~1MB image takes approx 107ms
#define BO_DFU_DOWNLOAD_MANIFEST_POLL_TIMEOUT_MS 1000

FORCE_INLINE_ATTR void IRAM_ATTR bo_dfu_update_state_impl(uint8_t current_state, bo_dfu_t *dfu, bo_dfu_fsm_t new_state, uint8_t status)
{
    switch(new_state)
    {
        case BO_DFU_FSM_IDLE:
            dfu->dfu.status_and_poll_timeout = 0;
            dfu->dfu.state_set = BO_DFU_SET_FSM(IDLE);
            break;
        case BO_DFU_FSM_DNLOAD_IDLE:
            dfu->dfu.status_and_poll_timeout = 0;
            dfu->dfu.state_set = BO_DFU_SET_FSM(DNLOAD_IDLE);
            break;
        case BO_DFU_FSM_DNLOAD_SYNC_READY:
            dfu->dfu.status_and_poll_timeout = BO_DFU_STATUS_AND_POLL_TIMEOUT32(0, BO_DFU_DOWNLOAD_SYNC_POLL_TIMEOUT_MS);
            dfu->dfu.state_set = BO_DFU_SET_FSM(DNLOAD_SYNC_READY);
            break;
        case BO_DFU_FSM_MANIFEST_SYNC_READY:
            dfu->dfu.status_and_poll_timeout = BO_DFU_STATUS_AND_POLL_TIMEOUT32(0, BO_DFU_DOWNLOAD_MANIFEST_POLL_TIMEOUT_MS);
            dfu->dfu.state_set = BO_DFU_SET_FSM(MANIFEST_SYNC_READY);
            break;
        case BO_DFU_FSM_DNLOAD_SYNC_DONE:
            dfu->dfu.status_and_poll_timeout = 0;
            dfu->dfu.state_set = BO_DFU_SET_FSM(DNLOAD_SYNC_DONE);
            break;
        case BO_DFU_FSM_MANIFEST_SYNC_DONE:
            dfu->dfu.status_and_poll_timeout = 0;
            dfu->dfu.state_set = BO_DFU_SET_FSM(MANIFEST_SYNC_DONE);
            break;
        case BO_DFU_FSM_ERROR:
            // CAREFUL: Setting dfu.state_set alters state_get/is_incomplete. The FSM must never return to < BO_DFU_FSM_MINIMUM_COMPLETE_VAL.
            if(!BO_DFU_FSM_IS_COMPLETE(current_state))
            {
                dfu->dfu.status_and_poll_timeout = BO_DFU_STATUS_AND_POLL_TIMEOUT32(status, 0);
                dfu->dfu.state_set = BO_DFU_SET_FSM(ERROR);
                break;
            }
            /* falls through */
        case BO_DFU_FSM_COMPLETE:
            dfu->dfu.status_and_poll_timeout = 0;
            dfu->dfu.state_set = BO_DFU_SET_FSM(COMPLETE);
            break;
        default:
            asm("error");
    }
}
#define bo_dfu_update_state_known(current_state, dfu, state, status) bo_dfu_update_state_impl(current_state, (dfu), BO_DFU_FSM(state), status)
#define bo_dfu_update_state(dfu, state, status) bo_dfu_update_state_impl(BO_DFU_T_GET_STATE(dfu), (dfu), BO_DFU_FSM(state), status)

static void IRAM_ATTR bo_dfu_usb_set_address(bo_dfu_t *dfu, uint32_t address)
{
    // Setting the 32b value (<=127) also clears configuration.
    dfu->address_and_configuration = address;
}

static void IRAM_ATTR bo_dfu_usb_set_configuration(bo_dfu_t *dfu, uint32_t configuration)
{
    dfu->configuration_value = configuration;
}

static IRAM_ATTR esp_err_t bo_dfu_verify_image_header(const esp_image_header_t *header)
{
    // see verify_image_header
    if(
        header->magic != ESP_IMAGE_HEADER_MAGIC ||
        ESP_OK != bootloader_common_check_chip_validity(header, ESP_IMAGE_APPLICATION) ||
        header->segment_count > ESP_IMAGE_MAX_SEGMENTS
    )
    {
        ESP_LOGE(BO_DFU_TAG, "[%s] invalid image header", __func__);
        return ESP_ERR_IMAGE_INVALID;
    }
    return ESP_OK;
}

static IRAM_ATTR esp_err_t bo_dfu_verify_image_app_desc(const esp_app_desc_t *app_desc)
{
    if(
        app_desc->magic_word != ESP_APP_DESC_MAGIC_WORD
    )
    {
        ESP_LOGE(BO_DFU_TAG, "[%s] invalid app_desc", __func__);
        return ESP_ERR_IMAGE_INVALID;
    }
    return ESP_OK;
}

static IRAM_ATTR usb_dfu_status_t bo_dfu_process_block(bo_dfu_t *dfu)
{
    if(dfu->dfu.block_num == 0)
    {
        // Perform some rudimentary file verification checks on the first block.
        struct {
            esp_image_header_t image_header;
            esp_image_segment_header_t segment_header;
            esp_app_desc_t app_desc;
        } *image = (typeof(image)) dfu->dfu.buffer;
        if(
            ESP_OK != bo_dfu_verify_image_header(&image->image_header) ||
            ESP_OK != bo_dfu_verify_image_app_desc(&image->app_desc)
        )
        {
            return BO_DFU_STATUS_errTARGET;
        }
    }

    const size_t write_sector = dfu->dfu.block_num_counter;
    const size_t write_offset = write_sector * 0x1000;
    const size_t write_size = sizeof(dfu->dfu.buffer);
    if(
        write_offset > dfu->ota.partition.size ||
        (write_offset + write_size) > dfu->ota.partition.size
    )
    {
        ESP_LOGE(BO_DFU_TAG, "[%s] address out of range (0x%X, 0x%x, 0x%X)", __func__, write_offset, write_size, dfu->ota.partition.size);
        return BO_DFU_STATUS_errADDRESS;
    }

    const size_t write_destination = dfu->ota.partition.offset + write_offset;
    ESP_LOGI(BO_DFU_TAG, "[%s] writing to 0x%08X", __func__, write_destination);
    if(ESP_OK != bootloader_flash_erase_range(write_destination, write_size))
    {
        ESP_LOGE(BO_DFU_TAG, "[%s] erase error", __func__);
        return BO_DFU_STATUS_errERASE;
    }
    if(ESP_OK != bootloader_flash_write(write_destination, dfu->dfu.buffer, write_size, false))
    {
        ESP_LOGE(BO_DFU_TAG, "[%s] write error", __func__);
        return BO_DFU_STATUS_errPROG;
    }
    return BO_DFU_STATUS_OK;
}

static IRAM_ATTR usb_dfu_status_t bo_dfu_process_firmware(bo_dfu_t *dfu)
{
    esp_image_metadata_t metadata;
    if(ESP_OK != esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &dfu->ota.partition, &metadata))
    {
        ESP_LOGE(BO_DFU_TAG, "[%s] image invalid", __func__);
        return BO_DFU_STATUS_errVERIFY;
    }

    if(ESP_OK != bootloader_flash_erase_sector(dfu->ota.entry_addr / 0x1000))
    {
        return BO_DFU_STATUS_errERASE;
    }

    if(ESP_OK != bootloader_flash_write(dfu->ota.entry_addr, &dfu->ota.entry, sizeof(dfu->ota.entry), false))
    {
        return BO_DFU_STATUS_errWRITE;
    }

    return BO_DFU_STATUS_OK;
}

static void IRAM_ATTR bo_dfu_usb_transaction_complete(bo_dfu_t *dfu)
{
    #define BREQUEST_AND_BMREQUESTTYPE(bRequest, bmRequestType) ((((uint16_t)(bRequest)) << 8) | (bmRequestType))
    switch(dfu->transfer.bmRequestType_and_bRequest)
    {
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_SET_ADDRESS, 0b00000000):
            ESP_LOGI(BO_DFU_TAG, "[%s] address assigned: 0x%02X", __func__, dfu->transfer.wValue);
            bo_dfu_usb_set_address(dfu, dfu->transfer.wValue);
            break;
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_GETSTATUS, 0b10100001):
        {
            const uint8_t current_dfu_fsm = BO_DFU_T_GET_STATE(dfu);
            switch(current_dfu_fsm)
            {
                case BO_DFU_FSM(DNLOAD_SYNC_READY):
                {
                    // -> DNBUSY
                    _Static_assert(sizeof(dfu->dfu.buffer) == 0x1000, "");
                    _Static_assert(offsetof(bo_dfu_t, dfu.buffer) == offsetof(bo_dfu_t, dfu.buffer_aligned), "");
                    if(((intptr_t)dfu->dfu.buffer % 4) != 0)
                    {
                        asm("alignment_error");
                    }

                    usb_dfu_status_t err = bo_dfu_process_block(dfu);
                    if(err != BO_DFU_STATUS_OK)
                    {
                        bo_dfu_update_state_known(current_dfu_fsm, dfu, ERROR, err);
                        break;
                    }
                    bo_dfu_update_state(dfu, DNLOAD_SYNC_DONE, BO_DFU_STATUS_OK);
                    break;
                }
                case BO_DFU_FSM(DNLOAD_SYNC_DONE):
                {
                    bo_dfu_update_state(dfu, DNLOAD_IDLE, BO_DFU_STATUS_OK);
                    ++dfu->dfu.block_num;
                    break;
                }
                case BO_DFU_FSM(MANIFEST_SYNC_READY):
                {
                    // -> MANIFEST
                    ESP_LOGI(BO_DFU_TAG, "[%s] verifying firmware", __func__);
                    usb_dfu_status_t err = bo_dfu_process_firmware(dfu);
                    if(err != BO_DFU_STATUS_OK)
                    {
                        bo_dfu_update_state_known(current_dfu_fsm, dfu, ERROR, err);
                        break;
                    }
                    ESP_LOGI(BO_DFU_TAG, "[%s] firmware verified", __func__);
                    bo_dfu_update_state(dfu, MANIFEST_SYNC_DONE, BO_DFU_STATUS_OK);
                    break;
                }
                case BO_DFU_FSM(MANIFEST_SYNC_DONE):
                {
                    ESP_LOGI(BO_DFU_TAG, "[%s] update complete", __func__);
                    bo_dfu_update_state(dfu, COMPLETE, BO_DFU_STATUS_OK);
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_DNLOAD, 0b00100001):
        {
            if(BO_DFU_T_GET_STATE(dfu) == BO_DFU_FSM(IDLE))
            {
                // First block, reset state.
                dfu->dfu.block_num = 0;
            }
            if(dfu->transfer.len < sizeof(dfu->dfu.buffer))
            {
                dfu->dfu.block_num_final = 1;
                if(dfu->transfer.len > 0)
                {
                    // Receiving a partial block. The entire sector will still be erased and written. Ensure unused bytes are cleared to 0xFF.
                    memset(dfu->dfu.buffer + dfu->transfer.len, 0xFF, sizeof(dfu->dfu.buffer) - dfu->transfer.len);
                }
            }
            if(dfu->transfer.len == 0)
            {
                bo_dfu_update_state(dfu, MANIFEST_SYNC_READY, BO_DFU_STATUS_OK);
            }
            else
            {
                bo_dfu_update_state(dfu, DNLOAD_SYNC_READY, BO_DFU_STATUS_OK);
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_SET_CONFIGURATION, 0b00000000):
            ESP_LOGI(BO_DFU_TAG, "[%s] configuration set: 0x%02X", __func__, dfu->transfer.wValue);
            bo_dfu_usb_set_configuration(dfu, dfu->transfer.wValue);
            break;
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_CLRSTATUS, 0b00100001):
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_ABORT, 0b00100001):
            bo_dfu_update_state(dfu, IDLE, BO_DFU_STATUS_OK);
            break;
        default:
            break;
    }
    #undef BREQUEST_AND_BMREQUESTTYPE
}

#endif /* BO_DFU_INTERNAL_H */

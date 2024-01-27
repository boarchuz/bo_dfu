#ifndef BO_DFU_OTA_H
#define BO_DFU_OTA_H

#include <stdint.h>
#include <string.h>

#include "esp_flash_partitions.h"
#include "bootloader_common.h"
#include "bootloader_utility.h"
#include "bootloader_flash.h"

#include "sdkconfig.h"

#ifndef SPI_SEC_SIZE
    #define SPI_SEC_SIZE 0x1000
#endif

esp_err_t bootloader_flash_write(size_t dest_addr, void *src, size_t size, bool write_encrypted);
esp_err_t bootloader_flash_erase_range(uint32_t start_addr, uint32_t size);
esp_err_t bootloader_flash_erase_sector(size_t sector);
const void *bootloader_mmap(uint32_t src_addr, uint32_t size);
void bootloader_munmap(const void *mapping);

typedef struct {
    esp_partition_pos_t partition;
    esp_ota_select_entry_t entry;
    uint32_t entry_addr;
} esp_bl_usb_ota_partition_t;

// ESP-IDF's read_otadata has internal linkage so is copied here
static IRAM_ATTR esp_err_t read_otadata(const esp_partition_pos_t *ota_info, esp_ota_select_entry_t *two_otadata)
{
    const esp_ota_select_entry_t *ota_select_map;
    if (ota_info->offset == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    if (ota_info->size < 2 * SPI_SEC_SIZE) {
        return ESP_FAIL;
    }

    ota_select_map = bootloader_mmap(ota_info->offset, ota_info->size);
    if (!ota_select_map) {
        return ESP_FAIL;
    }

    memcpy(&two_otadata[0], ota_select_map, sizeof(esp_ota_select_entry_t));
    memcpy(&two_otadata[1], (uint8_t *)ota_select_map + SPI_SEC_SIZE, sizeof(esp_ota_select_entry_t));
    bootloader_munmap(ota_select_map);
    return ESP_OK;
}

static esp_err_t IRAM_ATTR bo_dfu_ota_init(esp_bl_usb_ota_partition_t *ota_partition)
{
    bootloader_state_t bs = {0};
    esp_ota_select_entry_t otadata[2];
    if (
        !bootloader_utility_load_partition_table(&bs) ||
        bs.ota_info.offset == 0 ||
        bs.app_count == 0 ||
        read_otadata(&bs.ota_info, otadata) != ESP_OK
    )
    {
        return ESP_FAIL;
    }

    int next_ota_sector_index = 0;
    uint32_t ota_seq = 0;
    if(!bootloader_common_ota_select_invalid(&otadata[0]) || !bootloader_common_ota_select_invalid(&otadata[1]))
    {
        int current_ota_sector_index = bootloader_common_get_active_otadata(otadata);
        if(current_ota_sector_index != -1)
        {
            ota_seq = otadata[current_ota_sector_index].ota_seq;
            next_ota_sector_index = ((current_ota_sector_index + 1) % 2);
        }
    }

    // Prepare the new OTA entry so it can be written directly to flash if DFU completes
    ++ota_seq;
    memcpy(&ota_partition->partition, &bs.ota[(ota_seq - 1) % bs.app_count], sizeof(ota_partition->partition));
    ota_partition->entry_addr = bs.ota_info.offset + next_ota_sector_index * 0x1000;
    memset(&ota_partition->entry, 0xFF, sizeof(ota_partition->entry));
    ota_partition->entry.ota_seq = ota_seq;
    ota_partition->entry.ota_state = ESP_OTA_IMG_VALID;
    ota_partition->entry.crc = bootloader_common_ota_select_crc(&ota_partition->entry);
    return ESP_OK;
}

#endif /* BO_DFU_OTA_H */

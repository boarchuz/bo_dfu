/**
 * BO DFU Basic Example
 *
 * Hardware setup:
 *  - USB_D-: 25
 *  - USB_D+: 26
 *  - USB_EN: 27 (connect to D- through 1.5k resistor)
 *  - IO0 is used as a button to exit DFU mode. This is the PROG/BOOT button on a typical development board; you don't need to do anything.
 *
 * 1. Clone bo_dfu to your project's bootloader_components directory.
 * 2. Build and flash. The bootloader is now USB DFU capable. DFU mode will be entered on reset.
 * 3. Use dfu-util (dfu-util -D build/bo_dfu.bin) or https://boarchuz.github.io/bo_dfu_web/ (if supported by your device, OS, browser, etc) to update.
 * 4. When complete, the application will start. Note the time and partition info in your serial monitor:
 *      I (20781) main: App started!
 *      I (20781) main: Build time: Tue Jan 30 12:28:51 2024
        I (20791) main: Partition: ota_0 (0x00120000)
 * 5. Rebuild the application to generate a new binary with updated __TIMESTAMP__.
 * 6. Reset the ESP32 so it re-enters DFU mode.
 * 7. Update again as in 3.
 * 8. When complete, note the updated build time and running partition. USB DFU OTA is working correctly.
 *      I (25217) main: App started!
 *      I (25217) main: Build time: Tue Jan 30 12:29:57 2024
 *      I (25227) main: Partition: ota_1 (0x00220000)
 * 10. Reset the ESP32 so it re-enters DFU mode.
 * 11. Exit DFU mode without updating by grounding IO0 for 3 secs (hold the BOOT/PROG button on your development board).
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_ota_ops.h"

#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "App started!");
    ESP_LOGI(TAG, "Build time: %s", __TIMESTAMP__);
    ESP_LOGI(TAG, "Partition: %s (0x%08X)", running_partition->label, (unsigned int)running_partition->address);
    vTaskDelete(NULL);
}

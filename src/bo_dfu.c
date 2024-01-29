#include <stdint.h>

#include "esp_attr.h"

#include "bo_dfu.h"

#include "sdkconfig.h"

static IRAM_ATTR void bo_dfu_default(void)
{
    /**
     * This is a configurable reference implementation of the DFU loop.
     * It features a heartbeat LED, inactivity timeout, entry and exit buttons, etc which can be modified via Kconfig.
     * If you would like to customise the DFU process further (eg. by adding LED progress indicators), you can disable CONFIG_BO_DFU_DEFAULT
     * and use this as a starting point or template.
     *
     * Once initialised, the USB bus is maintained with a single function - bo_dfu_fsm - which must be called repeatedly as quickly as possible.
     * It should look something like this:
     *      [Initialisation]
     *      while(!dfu_complete) {
     *          [Miscellaneous Tasks]
     *          bo_dfu_fsm();
     *      }
     *      [Deinitialisation]
     *
     * A *very* small amount of time is available between bo_dfu_fsm() calls for other miscellaneous tasks, such as feeding the WDT or checking
     * timeout conditions. It's important to never block in this interval; even small delays may cause requests to be missed on the bus.
     *
     * The CPU is always configured to 240MHz for DFU mode, so the CPU cycle count can be used as an efficient timekeeping source.
     * However, this cycle counter is 32 bits, which means it will overflow every ~18s, which could limit its usefulness.
     * You might want to use another timer source if this is unsuitable, or take note of the inactivity timeout tracking here to see how
     * this can be used in conjunction with a "seconds" counter.
    */

    // Initialise USB GPIOs unconditionally to ensure they are in correct "detached" state even if DFU mode doesn't begin.
    bo_dfu_gpio_init();

    // A clean RTC reset is required. Entering DFU mode in other cases is untested.
    if(!bo_dfu_is_compatible_reset_type())
    {
        return;
    }

    #ifdef CONFIG_BO_DFU_DEFAULT_USE_ENTRY_BUTTON
        // Initialise entry button and check condition
        esp_rom_gpio_pad_select_gpio(CONFIG_BO_DFU_GPIO_ENTRY_BUTTON);
        #if CONFIG_BO_DFU_ENTRY_BUTTON_IDLE_LEVEL == 0
        gpio_pad_pulldown(CONFIG_BO_DFU_GPIO_ENTRY_BUTTON);
        #else
        gpio_pad_pullup(CONFIG_BO_DFU_GPIO_ENTRY_BUTTON);
        #endif
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[CONFIG_BO_DFU_GPIO_ENTRY_BUTTON]);
        if(gpio_ll_get_level(&GPIO, CONFIG_BO_DFU_GPIO_ENTRY_BUTTON) == CONFIG_BO_DFU_ENTRY_BUTTON_IDLE_LEVEL)
        {
            ESP_LOGI(BO_DFU_TAG, "DFU entry button released, skipping");
            return;
        }
    #endif

    ESP_LOGI(BO_DFU_TAG, "Starting DFU");

    bo_dfu_t dfu;
    if(ESP_OK != bo_dfu_init(&dfu))
    {
        ESP_LOGE(BO_DFU_TAG, "DFU init error");
        return;
    }

    ESP_LOGI(BO_DFU_TAG, "DFU Destination: 0x%x (size: 0x%X)", dfu.ota.partition.offset, dfu.ota.partition.size);

    #ifdef CONFIG_BO_DFU_DEFAULT_USE_HEARTBEAT_LED
        // Initialise heartbeat LED
        esp_rom_gpio_pad_select_gpio(CONFIG_BO_DFU_GPIO_HEARTBEAT_LED);
        #if CONFIG_BO_DFU_HEARTBEAT_LED_OFF_LEVEL == 0
        gpio_pad_pulldown(CONFIG_BO_DFU_GPIO_HEARTBEAT_LED);
        #else
        gpio_pad_pullup(CONFIG_BO_DFU_GPIO_HEARTBEAT_LED);
        #endif
        gpio_ll_output_enable(&GPIO, CONFIG_BO_DFU_GPIO_HEARTBEAT_LED);
        uint32_t led_previous_time = bo_dfu_ccount();
    #endif

    #ifdef CONFIG_BO_DFU_DEFAULT_USE_EXIT_BUTTON
        // Initialise exit button (skip if same as entry button as it's already initialised)
        #if !defined(CONFIG_BO_DFU_DEFAULT_USE_ENTRY_BUTTON) || CONFIG_BO_DFU_GPIO_ENTRY_BUTTON != CONFIG_BO_DFU_GPIO_EXIT_BUTTON
            esp_rom_gpio_pad_select_gpio(CONFIG_BO_DFU_GPIO_EXIT_BUTTON);
            #if CONFIG_BO_DFU_EXIT_BUTTON_IDLE_LEVEL == 0
            gpio_pad_pulldown(CONFIG_BO_DFU_GPIO_EXIT_BUTTON);
            #else
            gpio_pad_pullup(CONFIG_BO_DFU_GPIO_EXIT_BUTTON);
            #endif
            PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[CONFIG_BO_DFU_GPIO_EXIT_BUTTON]);
        #endif

        enum {
            BUTTON_IDLE     = CONFIG_BO_DFU_EXIT_BUTTON_IDLE_LEVEL,
            BUTTON_PRESSED  = !CONFIG_BO_DFU_EXIT_BUTTON_IDLE_LEVEL,
        } button_state = BUTTON_IDLE;
        uint32_t button_time;
    #endif

    #ifdef CONFIG_BO_DFU_DEFAULT_USE_INACTIVITY_TIMEOUT
        // Initialise inactivity timeout
        uint8_t inactivity_timeout_state = BO_DFU_T_GET_STATE(&dfu);
        uint32_t inactivity_timeout_count = 0;
        uint32_t inactivity_timeout_previous = bo_dfu_ccount();
    #endif

    #ifdef CONFIG_BO_DFU_DEFAULT_USE_CONNECT_TIMEOUT
        // Initialise connection timeout
        uint32_t attach_time = bo_dfu_ccount();
    #endif

    // Initialise completion timeout
    bool complete = false;
    uint32_t complete_time;

    #ifdef CONFIG_BOOTLOADER_WDT_ENABLE
        // Unlocking+Feeding+Locking the RTC WDT can be surprisingly slow, so it's left unlocked for the duration of the DFU loop for faster feeds.
        bo_dfu_wdt_unlock();
    #endif

    bo_dfu_usb_attach();
    for(;;)
    {
        #ifdef CONFIG_BOOTLOADER_WDT_ENABLE
            bo_dfu_wdt_feed();
        #endif

        #ifdef CONFIG_BO_DFU_DEFAULT_USE_HEARTBEAT_LED
            // Toggle LED every 250ms
            if(bo_dfu_ccount() - led_previous_time >= BO_DFU_MS_TO_CCOUNT(250))
            {
                #if CONFIG_BO_DFU_GPIO_HEARTBEAT_LED < 32
                    REG_WRITE(
                        (REG_READ(GPIO_OUT_REG) & (1 << CONFIG_BO_DFU_GPIO_HEARTBEAT_LED)) ? GPIO_OUT_W1TC_REG : GPIO_OUT_W1TS_REG,
                        (1 << CONFIG_BO_DFU_GPIO_HEARTBEAT_LED)
                    );
                #else
                    REG_WRITE(
                        (REG_READ(GPIO_OUT1_REG) & (1 << (CONFIG_BO_DFU_GPIO_HEARTBEAT_LED - 32))) ? GPIO_OUT1_W1TC_REG : GPIO_OUT1_W1TS_REG,
                        (1 << (CONFIG_BO_DFU_GPIO_HEARTBEAT_LED - 32))
                    );
                #endif
                led_previous_time += BO_DFU_MS_TO_CCOUNT(250);
            }
        #endif

        #ifdef CONFIG_BO_DFU_DEFAULT_USE_CONNECT_TIMEOUT
            // Check initial connection
            if(BO_DFU_T_IS_INIT(&dfu) && bo_dfu_ccount() - attach_time >= BO_DFU_MS_TO_CCOUNT(CONFIG_BO_DFU_DEFAULT_CONNECT_TIMEOUT_MS))
            {
                ESP_LOGI(BO_DFU_TAG, "Connection timed out, exiting");
                break;
            }
        #endif

        // Check complete
        if(!complete)
        {
            if(BO_DFU_T_IS_COMPLETE(&dfu))
            {
                ESP_LOGI(BO_DFU_TAG, "DFU download complete! Exiting in %u ms...", CONFIG_BO_DFU_COMPLETE_TIMEOUT_MS);
                complete = true;
                complete_time = bo_dfu_ccount();
            }
        }
        else
        {
            if(bo_dfu_ccount() - complete_time >= BO_DFU_MS_TO_CCOUNT(CONFIG_BO_DFU_COMPLETE_TIMEOUT_MS))
            {
                break;
            }
        }

        #ifdef CONFIG_BO_DFU_DEFAULT_USE_EXIT_BUTTON
            // Check exit button hold time
            if(gpio_ll_get_level(&GPIO, CONFIG_BO_DFU_GPIO_EXIT_BUTTON) != button_state)
            {
                button_state = !button_state;
                button_time = bo_dfu_ccount();
            }
            else if(
                button_state == BUTTON_PRESSED &&
                bo_dfu_ccount() - button_time >= BO_DFU_MS_TO_CCOUNT(CONFIG_BO_DFU_EXIT_BUTTON_HOLD_DURATION_MS)
            )
            {
                ESP_LOGI(BO_DFU_TAG, "Button held for %ums, exiting...", CONFIG_BO_DFU_EXIT_BUTTON_HOLD_DURATION_MS);
                break;
            }
        #endif

        #ifdef CONFIG_BO_DFU_DEFAULT_USE_INACTIVITY_TIMEOUT
            // Check inactivity timeout
            if(BO_DFU_T_GET_STATE(&dfu) != inactivity_timeout_state)
            {
                inactivity_timeout_state = BO_DFU_T_GET_STATE(&dfu);
                inactivity_timeout_count = 0;
                inactivity_timeout_previous = bo_dfu_ccount();
            }
            else if(bo_dfu_ccount() - inactivity_timeout_previous >= BO_DFU_MS_TO_CCOUNT(1000))
            {
                if(++inactivity_timeout_count >= CONFIG_BO_DFU_INACTIVITY_TIMEOUT_S)
                {
                    ESP_LOGI(BO_DFU_TAG, "DFU inactive, exiting");
                    break;
                }
                inactivity_timeout_previous += BO_DFU_MS_TO_CCOUNT(1000);
            }
        #endif

        bo_dfu_fsm(&dfu);
    }
    bo_dfu_usb_detach();

    #ifdef CONFIG_BOOTLOADER_WDT_ENABLE
        bo_dfu_wdt_lock();
    #endif

    #ifdef CONFIG_BO_DFU_DEFAULT_USE_HEARTBEAT_LED
        gpio_ll_output_disable(&GPIO, CONFIG_BO_DFU_GPIO_HEARTBEAT_LED);
    #endif

    bo_dfu_deinit();

    ESP_LOGI(BO_DFU_TAG, "DFU Ended. Updated: %d", BO_DFU_T_IS_COMPLETE(&dfu));
}

IRAM_ATTR void bootloader_after_init(void)
{
    bo_dfu_default();
}

#ifndef BO_DFU_GPIO_H
#define BO_DFU_GPIO_H

#include <assert.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp32/rom/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_io_reg.h"
#include "hal/gpio_types.h"
#include "hal/gpio_ll.h"
#include "esp_rom_gpio.h"

#include "sdkconfig.h"

_Static_assert(
    (CONFIG_BO_DFU_GPIO_DP < 32 && CONFIG_BO_DFU_GPIO_DN < 32) ||
    (CONFIG_BO_DFU_GPIO_DP >= 32 && CONFIG_BO_DFU_GPIO_DN >= 32),
    "USB D+/D- must be both <32 or both >=32"
);

_Static_assert(
    #ifdef CONFIG_BO_DFU_GPIO_EN
    CONFIG_BO_DFU_GPIO_EN <= 33 &&
    #endif
    CONFIG_BO_DFU_GPIO_DP <= 33 &&
    CONFIG_BO_DFU_GPIO_DN <= 33,
    "USB GPIOs must be output capable"
);

#if CONFIG_BO_DFU_GPIO_DP < 32
    #define BO_DFU_GPIO_DP          (CONFIG_BO_DFU_GPIO_DP)
    #define BO_DFU_GPIO_DN          (CONFIG_BO_DFU_GPIO_DN)
    #define BO_DFU_GPIO_REG_IN      GPIO_IN_REG
    #define BO_DFU_GPIO_REG_OUT     GPIO_OUT_REG
    #define BO_DFU_GPIO_REG_OUT_EN  GPIO_ENABLE_W1TS_REG
    #define BO_DFU_GPIO_REG_OUT_DIS GPIO_ENABLE_W1TC_REG
#else
    #define BO_DFU_GPIO_DP          (CONFIG_BO_DFU_GPIO_DP - 32)
    #define BO_DFU_GPIO_DN          (CONFIG_BO_DFU_GPIO_DN - 32)
    #define BO_DFU_GPIO_REG_IN      GPIO_IN1_REG
    #define BO_DFU_GPIO_REG_OUT     GPIO_OUT1_REG
    #define BO_DFU_GPIO_REG_OUT_EN  GPIO_ENABLE1_W1TS_REG
    #define BO_DFU_GPIO_REG_OUT_DIS GPIO_ENABLE1_W1TC_REG
#endif

// Despite adding more instructions, shifting values is a net benefit to size as the compiler will optimise further.
#define BO_DFU_GPIO_SHIFT (MIN(BO_DFU_GPIO_DP, BO_DFU_GPIO_DN))
#define BO_DFU_GPIO_MASK ((1 << BO_DFU_GPIO_DP) | (1 << BO_DFU_GPIO_DN))

typedef enum {
    BO_DFU_BUS_NLO_PLO = ((0 << BO_DFU_GPIO_DN) | (0 << BO_DFU_GPIO_DP)),
    BO_DFU_BUS_NLO_PHI = ((0 << BO_DFU_GPIO_DN) | (1 << BO_DFU_GPIO_DP)),
    BO_DFU_BUS_NHI_PLO = ((1 << BO_DFU_GPIO_DN) | (0 << BO_DFU_GPIO_DP)),
    BO_DFU_BUS_NHI_PHI = ((1 << BO_DFU_GPIO_DN) | (1 << BO_DFU_GPIO_DP)),

    BO_DFU_BUS_1 =      BO_DFU_BUS_NLO_PHI,
    BO_DFU_BUS_0 =      BO_DFU_BUS_NHI_PLO,
    BO_DFU_BUS_SE0 =    BO_DFU_BUS_NLO_PLO,
    BO_DFU_BUS_SE1 =    BO_DFU_BUS_NHI_PHI,

    // Low Speed
    BO_DFU_BUS_J = BO_DFU_BUS_0,
    BO_DFU_BUS_K = BO_DFU_BUS_1,
} bo_dfu_bus_t;

static IRAM_ATTR void bo_dfu_usb_attach(void)
{
    #ifdef CONFIG_BO_DFU_GPIO_EN
        #if CONFIG_BO_DFU_GPIO_EN < 32
            gpio_output_set(1 << CONFIG_BO_DFU_GPIO_EN, 0, 1 << CONFIG_BO_DFU_GPIO_EN, 0);
        #else
            gpio_output_set_high(1 << (CONFIG_BO_DFU_GPIO_EN - 32), 0, 1 << (CONFIG_BO_DFU_GPIO_EN - 32), 0);
        #endif
    #else
        /**
         * There is no enable pin to attach and detach from the bus; D- is pulled high externally.
         * Since the 3V3 rail is up well before we reach here, the host has probably already tried, failed, and given
         * up trying to contact this device.
         * The D- line should not be blindly driven low, to be clear, but it's the only option we have at this point
         * to put a detach state on the bus, hopefully prompting the host to retry the connection when released.
        */
        REG_WRITE(BO_DFU_GPIO_REG_OUT_EN, 1 << BO_DFU_GPIO_DN);
        esp_rom_delay_us(1000);
        REG_WRITE(BO_DFU_GPIO_REG_OUT_DIS, 1 << BO_DFU_GPIO_DN);
    #endif
}

static IRAM_ATTR void bo_dfu_usb_detach(void)
{
    #ifdef CONFIG_BO_DFU_GPIO_EN
        #if CONFIG_BO_DFU_GPIO_EN < 32
            gpio_output_set(0, 0, 0, 1 << CONFIG_BO_DFU_GPIO_EN);
        #else
            gpio_output_set_high(0, 0, 0, 1 << (CONFIG_BO_DFU_GPIO_EN - 32));
        #endif
    #endif
}

FORCE_INLINE_ATTR IRAM_ATTR void bo_dfu_gpio_disable_pullx(int gpio_num)
{
    const struct {
        uint32_t reg;
        uint32_t pullx_mask;
    } rtcio_pullx_regs[SOC_GPIO_PIN_COUNT] = {
        [0]  = {RTC_IO_TOUCH_PAD1_REG,      RTC_IO_TOUCH_PAD1_RUE_M |   RTC_IO_TOUCH_PAD1_RDE_M},
        [2]  = {RTC_IO_TOUCH_PAD2_REG,      RTC_IO_TOUCH_PAD2_RUE_M |   RTC_IO_TOUCH_PAD2_RDE_M},
        [4]  = {RTC_IO_TOUCH_PAD0_REG,      RTC_IO_TOUCH_PAD0_RUE_M |   RTC_IO_TOUCH_PAD0_RDE_M},
        [12] = {RTC_IO_TOUCH_PAD5_REG,      RTC_IO_TOUCH_PAD5_RUE_M |   RTC_IO_TOUCH_PAD5_RDE_M},
        [13] = {RTC_IO_TOUCH_PAD4_REG,      RTC_IO_TOUCH_PAD4_RUE_M |   RTC_IO_TOUCH_PAD4_RDE_M},
        [14] = {RTC_IO_TOUCH_PAD6_REG,      RTC_IO_TOUCH_PAD6_RUE_M |   RTC_IO_TOUCH_PAD6_RDE_M},
        [15] = {RTC_IO_TOUCH_PAD3_REG,      RTC_IO_TOUCH_PAD3_RUE_M |   RTC_IO_TOUCH_PAD3_RDE_M},
        [25] = {RTC_IO_PAD_DAC1_REG,        RTC_IO_PDAC1_RUE_M |        RTC_IO_PDAC1_RDE_M     },
        [26] = {RTC_IO_PAD_DAC2_REG,        RTC_IO_PDAC2_RUE_M |        RTC_IO_PDAC2_RDE_M     },
        [27] = {RTC_IO_TOUCH_PAD7_REG,      RTC_IO_TOUCH_PAD7_RUE_M |   RTC_IO_TOUCH_PAD7_RDE_M},
        [32] = {RTC_IO_XTAL_32K_PAD_REG,    RTC_IO_X32P_RUE_M |         RTC_IO_X32P_RDE_M      },
        [33] = {RTC_IO_XTAL_32K_PAD_REG,    RTC_IO_X32N_RUE_M |         RTC_IO_X32N_RDE_M      },
        [34] = {RTC_IO_ADC_PAD_REG,         0 |                         0                      },
        [35] = {RTC_IO_ADC_PAD_REG,         0 |                         0                      },
        [36] = {RTC_IO_SENSOR_PADS_REG,     0 |                         0                      },
        [37] = {RTC_IO_SENSOR_PADS_REG,     0 |                         0                      },
        [38] = {RTC_IO_SENSOR_PADS_REG,     0 |                         0                      },
        [39] = {RTC_IO_SENSOR_PADS_REG,     0 |                         0                      },
    };

    if(rtcio_pullx_regs[gpio_num].reg != 0)
    {
        REG_CLR_BIT(rtcio_pullx_regs[gpio_num].reg, rtcio_pullx_regs[gpio_num].pullx_mask);
    }
    else
    {
        const uint32_t GPIO_PIN_MUX_REG[] = {
            IO_MUX_GPIO0_REG,
            IO_MUX_GPIO1_REG,
            IO_MUX_GPIO2_REG,
            IO_MUX_GPIO3_REG,
            IO_MUX_GPIO4_REG,
            IO_MUX_GPIO5_REG,
            IO_MUX_GPIO6_REG,
            IO_MUX_GPIO7_REG,
            IO_MUX_GPIO8_REG,
            IO_MUX_GPIO9_REG,
            IO_MUX_GPIO10_REG,
            IO_MUX_GPIO11_REG,
            IO_MUX_GPIO12_REG,
            IO_MUX_GPIO13_REG,
            IO_MUX_GPIO14_REG,
            IO_MUX_GPIO15_REG,
            IO_MUX_GPIO16_REG,
            IO_MUX_GPIO17_REG,
            IO_MUX_GPIO18_REG,
            IO_MUX_GPIO19_REG,
            IO_MUX_GPIO20_REG,
            IO_MUX_GPIO21_REG,
            IO_MUX_GPIO22_REG,
            IO_MUX_GPIO23_REG,
            0,
            IO_MUX_GPIO25_REG,
            IO_MUX_GPIO26_REG,
            IO_MUX_GPIO27_REG,
            0,
            0,
            0,
            0,
            IO_MUX_GPIO32_REG,
            IO_MUX_GPIO33_REG,
            IO_MUX_GPIO34_REG,
            IO_MUX_GPIO35_REG,
            IO_MUX_GPIO36_REG,
            IO_MUX_GPIO37_REG,
            IO_MUX_GPIO38_REG,
            IO_MUX_GPIO39_REG,
        };
        REG_CLR_BIT(GPIO_PIN_MUX_REG[gpio_num], FUN_PU | FUN_PD);
    }
}

static IRAM_ATTR void bo_dfu_gpio_init(void)
{
    const int gpios[] = {
        CONFIG_BO_DFU_GPIO_DP,
        CONFIG_BO_DFU_GPIO_DN,
    };
    #pragma GCC unroll 2
    for(int i = 0; i < ARRAY_SIZE(gpios); ++i)
    {
        esp_rom_gpio_pad_select_gpio(gpios[i]);
        bo_dfu_gpio_disable_pullx(gpios[i]);
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[gpios[i]]);
    }

    #ifdef CONFIG_BO_DFU_GPIO_EN
    esp_rom_gpio_pad_select_gpio(CONFIG_BO_DFU_GPIO_EN);
    bo_dfu_gpio_disable_pullx(CONFIG_BO_DFU_GPIO_EN);
    #endif
}

#endif /* BO_DFU_GPIO_H */

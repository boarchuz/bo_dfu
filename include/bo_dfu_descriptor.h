#ifndef BO_DFU_DESCRIPTOR_H
#define BO_DFU_DESCRIPTOR_H

#include <wchar.h>

#include "esp_attr.h"
#include "hal/efuse_hal.h"
#include "soc/efuse_reg.h"

#include "bo_dfu_usb.h"
#include "bo_dfu_util.h"

#include "sdkconfig.h"

#define BO_DFU_DESCRIPTOR_ATTR static

#define STRINGIFY16_(x) u##x
#define STRINGIFY16(x) STRINGIFY16_(x)

#define BO_DFU_SERIAL_MAC_LEN 6
#define BO_DFU_TRANSFER_SIZE (0x1000)

enum {
    BO_DFU_DESCRIPTOR_STRING_INDEX_LANGID = 0,
    BO_DFU_DESCRIPTOR_STRING_INDEX_MANUFACTURER,
    BO_DFU_DESCRIPTOR_STRING_INDEX_DEVICE,
    BO_DFU_DESCRIPTOR_STRING_INDEX_SERIAL,
    BO_DFU_DESCRIPTOR_STRING_INDEX_INTERFACE,
};

BO_DFU_DESCRIPTOR_ATTR struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wLangIds[1];
} g_usb_descriptor_string_langid = {
    .bLength = sizeof(g_usb_descriptor_string_langid),
    .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_STRING,
    .wLangIds = {
        // See http://www.baiheee.com/Documents/090518/090518112619/USB_LANGIDs.pdf
        #define BO_DFU_DESCRIPTOR_STRING_LANG_ID_ENGLISH_USA 0x0409
        BO_DFU_DESCRIPTOR_STRING_LANG_ID_ENGLISH_USA,
    },
};

BO_DFU_DESCRIPTOR_ATTR struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t string[sizeof(CONFIG_BO_DFU_DEVICE_NAME) - 1];
} g_usb_descriptor_string_device = {
    .bLength = sizeof(g_usb_descriptor_string_device),
    .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_STRING,
    .string = STRINGIFY16(CONFIG_BO_DFU_DEVICE_NAME),
};

BO_DFU_DESCRIPTOR_ATTR struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t string[sizeof(CONFIG_BO_DFU_INTERFACE_NAME) - 1];
} g_usb_descriptor_string_interface = {
    .bLength = sizeof(g_usb_descriptor_string_interface),
    .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_STRING,
    .string = STRINGIFY16(CONFIG_BO_DFU_INTERFACE_NAME),
};

BO_DFU_DESCRIPTOR_ATTR struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t string[sizeof(CONFIG_BO_DFU_MANUFACTURER_NAME) - 1];
} g_usb_descriptor_string_manufacturer = {
    .bLength = sizeof(g_usb_descriptor_string_manufacturer),
    .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_STRING,
    .string = STRINGIFY16(CONFIG_BO_DFU_MANUFACTURER_NAME),
};

BO_DFU_DESCRIPTOR_ATTR struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    // 2 characters for each MAC byte
    uint16_t string[2 * BO_DFU_SERIAL_MAC_LEN];
} g_usb_string_descriptor_serial = {
    .bLength = sizeof(g_usb_string_descriptor_serial),
    .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_STRING,
};

BO_DFU_DESCRIPTOR_ATTR bo_dfu_usb_device_descriptor_t g_usb_descriptor_device = {
    .bLength = sizeof(bo_dfu_usb_device_descriptor_t),
    .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_DEVICE,
    .bcdUSB = 0x0100,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = BO_DFU_USB_LOW_SPEED_PACKET_SIZE,
    .idVendor = CONFIG_BO_DFU_VENDOR_ID,
    .idProduct = CONFIG_BO_DFU_PRODUCT_ID,
    #ifdef CONFIG_BO_DFU_BCD_CUSTOM
    .bcdDevice = CONFIG_BO_DFU_BCD,
    #else
    .bcdDevice = 0, // Set at runtime
    #endif
    .iManufacturer = BO_DFU_DESCRIPTOR_STRING_INDEX_MANUFACTURER,
    .iProduct = BO_DFU_DESCRIPTOR_STRING_INDEX_DEVICE,
    .iSerialNumber = BO_DFU_DESCRIPTOR_STRING_INDEX_SERIAL,
    .bNumConfigurations = 1,
};

BO_DFU_DESCRIPTOR_ATTR struct {
    bo_dfu_usb_configuration_descriptor_t configuration;
    bo_dfu_usb_interface_descriptor_t interfaces[1];
    bo_dfu_functional_descriptor_t dfu_functional_descriptor;
} g_usb_descriptor_configuration = {
    .configuration = {
        .bLength = sizeof(bo_dfu_usb_configuration_descriptor_t),
        .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_CONFIGURATION,
        .wTotalLength = sizeof(g_usb_descriptor_configuration),
        .bNumInterfaces = ARRAY_SIZE(g_usb_descriptor_configuration.interfaces),
        .bConfigurationValue = 1,
        .iConfiguration = 0,
        .bmAttributes = BIT(7),
        .MaxPower = MIN(CONFIG_BO_DFU_MAX_POWER_MA, 500) / 2, // Maximum power consumption (in 2mA units), up to 500mA
    },
    .interfaces = {
        {
            // DFU Interface
            .bLength = sizeof(bo_dfu_usb_interface_descriptor_t),
            .bDescriptorType = BO_DFU_USB_DESCRIPTOR_TYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 0,
            // https://www.usb.org/defined-class-codes
            .bInterfaceClass = 0xFE,
            .bInterfaceSubClass = 0x01,
            .bInterfaceProtocol = 0x02,
            .iInterface = BO_DFU_DESCRIPTOR_STRING_INDEX_INTERFACE,
        },
    },
    .dfu_functional_descriptor = {
        .bLength = sizeof(bo_dfu_functional_descriptor_t),
        .bDescriptorType = 0x21,
        .bmAttributes = (BO_DFU_FUNCTIONAL_ATTR_CAN_DOWNLOAD | BO_DFU_FUNCTIONAL_ATTR_MANIFEST_TOLERANT),
        .wDetachTimeout = 0,
        .wTransferSize = BO_DFU_TRANSFER_SIZE,
        .bcdDFUVersion = 0x0100,
    },
};

static IRAM_ATTR uint16_t bo_dfu_nibble_to_hex_char16(uint8_t n)
{
    if (n > 9) {
        return n - 10 + 'a';
    }
    return n + '0';
}

static IRAM_ATTR void bo_dfu_descriptor_serial_init(void)
{
    const uint8_t *mac_byte_ptr = (const uint8_t*)(EFUSE_BLK0_RDATA1_REG);
    uint16_t *dest = g_usb_string_descriptor_serial.string;
    for(int i = 0; i < BO_DFU_SERIAL_MAC_LEN; ++i)
    {
        dest[i * 2] = bo_dfu_nibble_to_hex_char16(mac_byte_ptr[(BO_DFU_SERIAL_MAC_LEN - 1) - i] >> 4);
        dest[i * 2 + 1] = bo_dfu_nibble_to_hex_char16(mac_byte_ptr[(BO_DFU_SERIAL_MAC_LEN - 1) - i] & 0xf);
    }
}

static IRAM_ATTR void bo_dfu_descriptor_bcd_init(void)
{
    #ifndef CONFIG_BO_DFU_BCD_CUSTOM
        uint16_t bcd = {
            ((efuse_hal_get_major_chip_version() & 0xFF) << 8) |
            ((efuse_hal_get_minor_chip_version() & 0xFF) << 0)
        };
        g_usb_descriptor_device.bcdDevice = bcd;
    #endif
}

static IRAM_ATTR void bo_dfu_descriptor_init(void)
{
    bo_dfu_descriptor_serial_init();
    bo_dfu_descriptor_bcd_init();
}

#endif /* BO_DFU_DESCRIPTOR_H */

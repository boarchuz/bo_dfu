#ifndef BO_DFU_USB_H
#define BO_DFU_USB_H

#include <assert.h>
#include <stdint.h>

#include "sdkconfig.h"

#define BO_DFU_USB_SYNC_BYTE (0b10000000)
#define BO_DFU_USB_LOW_SPEED_FREQ_HZ 1500000
#define BO_DFU_USB_LOW_SPEED_PACKET_SIZE 8
#define BO_DFU_USB_RESET_SIGNAL_NS 2500

#define BO_DFU_FUNCTIONAL_ATTR_WILL_DETACH         (1 << 3)
#define BO_DFU_FUNCTIONAL_ATTR_MANIFEST_TOLERANT   (1 << 2)
#define BO_DFU_FUNCTIONAL_ATTR_CAN_UPLOAD          (1 << 1)
#define BO_DFU_FUNCTIONAL_ATTR_CAN_DOWNLOAD        (1 << 0)

#define BO_DFU_USB_MAX_VALID_ADDRESS 127

// 7.1.18 Bus Turn-around Time and Inter-packet Delay
// 8.7.2 Bus Turn-around Timing
#define BO_DFU_USB_WAIT_ACK_TIMEOUT_BIT_TIMES 16
#define BO_DFU_USB_WAIT_DATA_TIMEOUT_BIT_TIMES 16

typedef enum {
    BO_DFU_BREQUEST_DNLOAD = 1,
    BO_DFU_BREQUEST_GETSTATUS = 3,
    BO_DFU_BREQUEST_CLRSTATUS = 4,
    BO_DFU_BREQUEST_GETSTATE = 5,
    BO_DFU_BREQUEST_ABORT = 6,
} bo_dfu_brequest_t;

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bmAttributes;
    uint16_t wDetachTimeout;
    uint16_t wTransferSize;
    uint16_t bcdDFUVersion;
} bo_dfu_functional_descriptor_t;
_Static_assert(sizeof(bo_dfu_functional_descriptor_t) == 9, "");

typedef enum {
    BO_DFU_STATUS_OK = 0x00, // No error condition is present.
    BO_DFU_STATUS_errTARGET = 0x01, // File is not targeted for use by this device.
    BO_DFU_STATUS_errFILE = 0x02, // File is for this device but fails some vendor-specific verification test.
    BO_DFU_STATUS_errWRITE = 0x03, // Device is unable to write memory.
    BO_DFU_STATUS_errERASE = 0x04, // Memory erase function failed.
    BO_DFU_STATUS_errCHECK_ERASED = 0x05, // Memory erase check failed.
    BO_DFU_STATUS_errPROG = 0x06, // Program memory function failed.
    BO_DFU_STATUS_errVERIFY = 0x07, // Programmed memory failed verification.
    BO_DFU_STATUS_errADDRESS = 0x08, // Cannot program memory due to received address that is out of range.
    BO_DFU_STATUS_errNOTDONE = 0x09, // Received DFU_DNLOAD with wLength = 0, but device does not think it has all of the data yet.
    BO_DFU_STATUS_errFIRMWARE = 0x0A, // Device’s firmware is corrupt. It cannot return to run-time (non-DFU) operations.
    BO_DFU_STATUS_errVENDOR = 0x0B, // iString indicates a vendor-specific error.
    BO_DFU_STATUS_errUSBR = 0x0C, // Device detected unexpected USB reset signaling.
    BO_DFU_STATUS_errPOR = 0x0D, // Device detected unexpected power on reset.
    BO_DFU_STATUS_errUNKNOWN = 0x0E, // Something went wrong, but the device does not know what it was.
    BO_DFU_STATUS_errSTALLEDPKT = 0x0F, // Device stalled an unexpected request.
} usb_dfu_status_t;

typedef enum {
    BO_DFU_STATE_PROTOCOL_appIDLE                   = 0,
    BO_DFU_STATE_PROTOCOL_appDETACH                 = 1,
    BO_DFU_STATE_PROTOCOL_dfuIDLE                   = 2,
    BO_DFU_STATE_PROTOCOL_dfuDNLOAD_SYNC            = 3,
    BO_DFU_STATE_PROTOCOL_dfuDNBUSY                 = 4,
    BO_DFU_STATE_PROTOCOL_dfuDNLOAD_IDLE            = 5,
    BO_DFU_STATE_PROTOCOL_dfuMANIFEST_SYNC          = 6,
    BO_DFU_STATE_PROTOCOL_dfuMANIFEST               = 7,
    BO_DFU_STATE_PROTOCOL_dfuMANIFEST_WAIT_RESET    = 8,
    BO_DFU_STATE_PROTOCOL_dfuUPLOAD_IDLE            = 9,
    BO_DFU_STATE_PROTOCOL_dfuERROR                  = 10,
} bo_dfu_state_protocol_t;

typedef struct __attribute__((packed)) {
    uint32_t bStatus : 8;
    uint32_t bwPollTimeout : 24;
    uint8_t bState;
    uint8_t iString;
} bo_dfu_get_status_response_t;
_Static_assert(sizeof(bo_dfu_get_status_response_t) == 6, "");

typedef enum {
    BO_DFU_USB_BREQUEST_GET_STATUS = 0,
    BO_DFU_USB_BREQUEST_SET_ADDRESS = 5,
    BO_DFU_USB_BREQUEST_GET_DESCRIPTOR = 6,
    BO_DFU_USB_BREQUEST_GET_CONFIGURATION = 8,
    BO_DFU_USB_BREQUEST_SET_CONFIGURATION = 9,
    BO_DFU_USB_BREQUEST_GET_INTERFACE = 10,
    BO_DFU_USB_BREQUEST_SET_INTERFACE = 11,
} bo_dfu_usb_brequest_t;

typedef enum {
    BO_DFU_USB_DESCRIPTOR_TYPE_DEVICE = 1,
    BO_DFU_USB_DESCRIPTOR_TYPE_CONFIGURATION = 2,
    BO_DFU_USB_DESCRIPTOR_TYPE_STRING = 3,
    BO_DFU_USB_DESCRIPTOR_TYPE_INTERFACE = 4,
} bo_dfu_usb_descriptor_types_t;

typedef enum {
    BO_DFU_USB_PID_CHECK_OUT =   0b11100001,
    BO_DFU_USB_PID_CHECK_IN =    0b01101001,
    BO_DFU_USB_PID_CHECK_SETUP = 0b00101101,
    BO_DFU_USB_PID_CHECK_DATA0 = 0b11000011,
    BO_DFU_USB_PID_CHECK_DATA1 = 0b01001011,
    BO_DFU_USB_PID_CHECK_ACK =   0b11010010,
    BO_DFU_USB_PID_CHECK_STALL = 0b00011110,
} bo_dfu_usb_pid_check_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} bo_dfu_usb_device_descriptor_t;
_Static_assert(sizeof(bo_dfu_usb_device_descriptor_t) == 18, "");

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes; // D7: Reserved (set to one)
    uint8_t MaxPower;
} bo_dfu_usb_configuration_descriptor_t;
_Static_assert(sizeof(bo_dfu_usb_configuration_descriptor_t) == 9, "");

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} bo_dfu_usb_interface_descriptor_t;
_Static_assert(sizeof(bo_dfu_usb_interface_descriptor_t) == 9, "");

#endif /* BO_DFU_USB_H */

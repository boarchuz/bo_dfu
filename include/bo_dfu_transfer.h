#ifndef BO_DFU_USB_TRANSFER_H
#define BO_DFU_USB_TRANSFER_H

#include <stdint.h>

#include "esp_attr.h"

#include "bo_dfu_tx.h"
#include "bo_dfu_rx.h"
#include "bo_dfu_crc.h"

#include "bo_dfu_internal_types.h"
#include "bo_dfu_usb.h"
#include "bo_dfu_util.h"
#include "bo_dfu_descriptor.h"
#include "bo_dfu_time.h"

static IRAM_ATTR bool bo_dfu_usb_transaction_check_token(uint16_t address, const bo_dfu_usb_rx_packet_t *packet, size_t len)
{
    if(len != (sizeof(packet->sync_and_pid_with_check) + sizeof(packet->token)))
    {
        return false;
    }
    if(packet->sync != BO_DFU_USB_SYNC_BYTE)
    {
        return false;
    }
    if(packet->address != address)
    {
        return false;
    }
    switch(packet->pid_with_check)
    {
        case BO_DFU_USB_PID_CHECK_SETUP:
        case BO_DFU_USB_PID_CHECK_OUT:
        case BO_DFU_USB_PID_CHECK_IN:
        {
            if(bo_dfu_crc_token(packet->token) != packet->crc)
            {
                return false;
            }
            break;
        }
        default:
        {
            return false;
        }
    }
    return true;
}

static IRAM_ATTR bool bo_dfu_usb_transaction_check_ack(const bo_dfu_usb_rx_packet_t *packet, size_t len)
{
    return (
        len == sizeof(packet->sync_and_pid_with_check) &&
        packet->sync == BO_DFU_USB_SYNC_BYTE &&
        packet->pid_with_check == BO_DFU_USB_PID_CHECK_ACK
    );
}

static IRAM_ATTR bool bo_dfu_usb_transaction_check_data(const bo_dfu_usb_rx_packet_t *packet, size_t len)
{
    if(len < (sizeof(uint8_t) + sizeof(uint8_t) /* + data */ + sizeof(uint16_t)))
    {
        return false;
    }
    if(packet->sync != BO_DFU_USB_SYNC_BYTE)
    {
        return false;
    }
    switch(packet->pid_with_check)
    {
        case BO_DFU_USB_PID_CHECK_DATA0:
        case BO_DFU_USB_PID_CHECK_DATA1:
        {
            return (
                len >= ((sizeof(uint8_t) + sizeof(uint8_t) /* no data */ + 0 + sizeof(uint16_t))) &&
                len <=((sizeof(uint8_t) + sizeof(uint8_t) /* max data */ + BO_DFU_USB_LOW_SPEED_PACKET_SIZE + sizeof(uint16_t))) &&
                bo_dfu_crc_data(packet->data, (len - ((sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t))))) == *(uint16_t*)(&packet->data[(len - ((sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t))))])
            );
        }
        default:
            break;
    }
    return false;
}

static IRAM_ATTR bool bo_dfu_usb_transaction_setup_check(bo_dfu_t *dfu, const bo_dfu_usb_rx_packet_t *packet, const void **data_to_send, size_t *data_len)
{
    #define BREQUEST_AND_BMREQUESTTYPE(bRequest, bmRequestType) ((((uint16_t)(bRequest)) << 8) | (bmRequestType))
    #define WINDEX_AND_WLENGTH(wIndex, wLength) (((wLength) << 16) | ((wIndex) << 0))
    #define WINDEX_AND_WLENGTH_CHECK(comparison, wIndex, wLength) (packet->setup_data.wIndex_and_wLength comparison WINDEX_AND_WLENGTH(wIndex, wLength))
    switch(packet->setup_data.bmRequestType_and_bRequest)
    {
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_GET_DESCRIPTOR, 0b10000000):
        {
            if(packet->setup_data.get_descriptor.type == BO_DFU_USB_DESCRIPTOR_TYPE_DEVICE && packet->setup_data.wIndex == 0 && packet->setup_data.get_descriptor.index == 0)
            {
                *data_to_send = &g_usb_descriptor_device;
                *data_len = sizeof(g_usb_descriptor_device);
                return true;
            }
            else if(packet->setup_data.get_descriptor.type == BO_DFU_USB_DESCRIPTOR_TYPE_CONFIGURATION && packet->setup_data.wIndex == 0 && packet->setup_data.get_descriptor.index == 0)
            {
                *data_to_send = &g_usb_descriptor_configuration;
                *data_len = sizeof(g_usb_descriptor_configuration);
                return true;
            }
            else if(packet->setup_data.get_descriptor.type == BO_DFU_USB_DESCRIPTOR_TYPE_STRING)
            {
                if(packet->setup_data.get_descriptor.index == BO_DFU_DESCRIPTOR_STRING_INDEX_LANGID)
                {
                    *data_to_send = &g_usb_descriptor_string_langid;
                    *data_len = sizeof(g_usb_descriptor_string_langid);
                    return true;
                }
                else if(packet->setup_data.get_descriptor.index == BO_DFU_DESCRIPTOR_STRING_INDEX_MANUFACTURER)
                {
                    *data_to_send = &g_usb_descriptor_string_manufacturer;
                    *data_len = sizeof(g_usb_descriptor_string_manufacturer);
                    return true;
                }
                else if(packet->setup_data.get_descriptor.index == BO_DFU_DESCRIPTOR_STRING_INDEX_DEVICE)
                {
                    *data_to_send = &g_usb_descriptor_string_device;
                    *data_len = sizeof(g_usb_descriptor_string_device);
                    return true;
                }
                else if(packet->setup_data.get_descriptor.index == BO_DFU_DESCRIPTOR_STRING_INDEX_SERIAL)
                {
                    *data_to_send = &g_usb_string_descriptor_serial;
                    *data_len = sizeof(g_usb_string_descriptor_serial);
                    return true;
                }
                else if(packet->setup_data.get_descriptor.index == BO_DFU_DESCRIPTOR_STRING_INDEX_INTERFACE)
                {
                    *data_to_send = &g_usb_descriptor_string_interface;
                    *data_len = sizeof(g_usb_descriptor_string_interface);
                    return true;
                }
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_SET_ADDRESS, 0b00000000):
        {
            if(
                packet->setup_data.wValue <= BO_DFU_USB_MAX_VALID_ADDRESS &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, 0)
            )
            {
                return true;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_GET_CONFIGURATION, 0b10000000):
        {
            if(
                BO_DFU_IS_ADDRESSED(dfu) &&
                packet->setup_data.wValue == 0 &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, sizeof(uint8_t))
            )
            {
                *data_to_send = &dfu->configuration_value;
                *data_len = sizeof(uint8_t);
                return true;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_GET_STATUS, 0b10000000):
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_GET_STATUS, 0b10000001):
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_GET_STATUS, 0b10000010):
        {
            if(
                BO_DFU_IS_ADDRESSED(dfu) &&
                packet->setup_data.wValue == 0 &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, sizeof(uint16_t))
            )
            {
                *data_to_send = &dfu->get_status_response_buffer;
                *data_len = sizeof(dfu->get_status_response_buffer);
                return true;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_SET_CONFIGURATION, 0b00000000):
        {
            if(
                BO_DFU_IS_ADDRESSED(dfu) &&
                packet->setup_data.wValue <= 1 &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, 0)
            )
            {
                return true;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_GET_INTERFACE, 0b10000001):
        {
            if(
                BO_DFU_IS_CONFIGURED(dfu) &&
                packet->setup_data.wValue == 0 &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, sizeof(uint8_t))
            )
            {
                *data_to_send = &dfu->get_interface_response_buffer;
                *data_len = sizeof(dfu->get_interface_response_buffer);
                return true;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_USB_BREQUEST_SET_INTERFACE, 0b00000001):
        {
            if(
                BO_DFU_IS_CONFIGURED(dfu) &&
                packet->setup_data.wIndex == 0 &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, 0)
            )
            {
                return true;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_GETSTATUS, 0b10100001):
        {
            if(
                BO_DFU_IS_CONFIGURED(dfu) &&
                packet->setup_data.wValue == 0 &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, sizeof(bo_dfu_get_status_response_t))
            )
            {
                *data_to_send = &dfu->dfu.status_response;
                *data_len = sizeof(bo_dfu_get_status_response_t);
                return true;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_DNLOAD, 0b00100001):
        {
            switch(BO_DFU_T_GET_STATE(dfu))
            {
                case BO_DFU_FSM(IDLE):
                case BO_DFU_FSM(DNLOAD_IDLE):
                {
                    /**
                     * Per the DFU spec, the host may select any block size between bMaxPacketSize0 and the device's specified wTransferSize.
                     * However, this application is simplified greatly by requiring wTransferSize, as it is set to the flash sector size, and
                     * thus each block can be easily processed by erasing and writing a sector of flash.
                     * The host will typically default to wTransferSize so this is unlikely to ever be an issue. In such a case, the user may
                     * need to specify the block size to force the DFU program to use the suggested wTransferSize.
                     *
                     * For the same reasons, this application rejects blocks smaller than this size.
                     * This would normally occur on the last block when the firmware size is not an exact multiple of wTransferSize.
                     * It is therefore necessary to pad the firmware file to the next wTransferSize boundary.
                    */
                    if(
                        BO_DFU_IS_CONFIGURED(dfu) &&
                        (
                            (
                                // If IDLE and this is the first packet (wValue == 0)
                                BO_DFU_T_GET_STATE(dfu) == BO_DFU_FSM(IDLE) &&
                                packet->setup_data.wValue == 0 &&
                                WINDEX_AND_WLENGTH_CHECK(==, 0, sizeof(dfu->dfu.buffer))
                            ) ||
                            (
                                // Or if DNLOAD_IDLE and...
                                BO_DFU_T_GET_STATE(dfu) == BO_DFU_FSM(DNLOAD_IDLE) && (
                                    (
                                        // This is the next packet (wValue == block_num), more data or null
                                        dfu->dfu.block_num_final == 0 &&
                                        packet->setup_data.wValue == dfu->dfu.block_num &&
                                        WINDEX_AND_WLENGTH_CHECK(<=, 0, sizeof(dfu->dfu.buffer))
                                    ) ||
                                    (
                                        // Or previous packet was < MaxPacketSize (ie. last data packet) and this is the null packet
                                        dfu->dfu.block_num_final == 1 &&
                                        packet->setup_data.wValue == dfu->dfu.block_num_counter &&
                                        WINDEX_AND_WLENGTH_CHECK(==, 0, 0)
                                    )
                                )
                             )
                        )
                    )
                    {
                        *data_len = packet->setup_data.wLength;
                        return true;
                    }
                    ESP_LOGW(BO_DFU_TAG, "[%s] invalid dnload (%u %u %u %u %u)", __func__, BO_DFU_T_GET_STATE(dfu), packet->setup_data.wValue, dfu->dfu.block_num, packet->setup_data.wIndex, packet->setup_data.wLength);
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_CLRSTATUS, 0b00100001):
        {
            switch(BO_DFU_T_GET_STATE(dfu))
            {
                case BO_DFU_FSM(ERROR):
                {
                    if(
                        BO_DFU_IS_CONFIGURED(dfu) &&
                        packet->setup_data.wValue == 0 &&
                        WINDEX_AND_WLENGTH_CHECK(==, 0, 0)
                    )
                    {
                        return true;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_ABORT, 0b00100001):
        {
            switch(BO_DFU_T_GET_STATE(dfu))
            {
                case BO_DFU_FSM(IDLE):
                case BO_DFU_FSM(DNLOAD_IDLE):
                {
                    if(
                        BO_DFU_IS_CONFIGURED(dfu) &&
                        packet->setup_data.wValue == 0 &&
                        WINDEX_AND_WLENGTH_CHECK(==, 0, 0)
                    )
                    {
                        return true;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case BREQUEST_AND_BMREQUESTTYPE(BO_DFU_BREQUEST_GETSTATE, 0b10100001):
        {
            if(
                BO_DFU_IS_CONFIGURED(dfu) &&
                packet->setup_data.wValue == 0 &&
                WINDEX_AND_WLENGTH_CHECK(==, 0, sizeof(uint8_t))
            )
            {
                *data_to_send = &dfu->dfu.get_state_response;
                *data_len = sizeof(uint8_t);
                return true;
            }
            break;
        }
    }
    #undef WINDEX_AND_WLENGTH
    #undef WINDEX_AND_WLENGTH_CHECK
    #undef BREQUEST_AND_BMREQUESTTYPE
    return false;
}

static IRAM_ATTR bool bo_dfu_usb_transaction_setup(bo_dfu_t *dfu, const bo_dfu_usb_rx_packet_t *packet, bo_dfu_usb_transfer_t *transfer)
{
    const void *data_ptr = NULL;
    size_t data_len = 0;
    if(!bo_dfu_usb_transaction_setup_check(dfu, packet, &data_ptr, &data_len))
    {
        return false;
    }
    transfer->bmRequestType_and_bRequest = packet->setup_data.bmRequestType_and_bRequest;
    transfer->wValue = packet->setup_data.wValue;
    transfer->data = data_ptr;
    transfer->len = MIN(data_len, packet->setup_data.wLength);
    transfer->counter = 0;
    _Static_assert(
        sizeof(*transfer) == (
            sizeof(transfer->bmRequestType_and_bRequest) +
            sizeof(transfer->wValue) + sizeof(transfer->data) +
            sizeof(transfer->len) +
            sizeof(transfer->counter)
        ), ""
    );
    return true;
}

static IRAM_ATTR bo_dfu_bus_state_t bo_dfu_usb_transaction_next(bo_dfu_t *dfu)
{
    enum {
        TRANSACTION_STATE_NONE,
        TRANSACTION_STATE_WAITING_ACK,
        TRANSACTION_STATE_SETUP_WAITING_DATA = BO_DFU_USB_PID_CHECK_SETUP,
        TRANSACTION_STATE_OUT_WAITING_DATA = BO_DFU_USB_PID_CHECK_OUT,
    };
    typeof(((bo_dfu_usb_rx_packet_t*)0)->pid_with_check) transaction_state = TRANSACTION_STATE_NONE;

    uint32_t bit_time = bo_dfu_ccount();
    do {
        bo_dfu_usb_rx_packet_t packet;
        int bytes_received = bo_dfu_usb_rx_next_packet(&bit_time, &packet);
        if(bytes_received < 0)
        {
            return bytes_received;
        }

        switch(transaction_state)
        {
            case TRANSACTION_STATE_NONE:
            {
                if(!bo_dfu_usb_transaction_check_token(dfu->address, &packet, bytes_received))
                {
                    return BO_DFU_BUS_SYNCED;
                }
                if(packet.endpoint != 0)
                {
                    bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_STALL);
                    return BO_DFU_BUS_SYNCED;
                }
                // PID is confirmed to be IN/OUT/SETUP in bo_dfu_usb_transaction_check_token.
                if(packet.pid_with_check != BO_DFU_USB_PID_CHECK_IN)
                {
                    // If OUT/SETUP, set state and loop to receive followup data packet.
                    _Static_assert(sizeof(transaction_state) >= sizeof(packet.pid_with_check), "");
                    transaction_state = packet.pid_with_check;
                    continue;
                }
                // IN
                if(!dfu->transfer.request_is_active)
                {
                    // Transaction is inactive (waiting for SETUP)
                    bo_dfu_update_state(dfu, ERROR, BO_DFU_STATUS_errSTALLEDPKT);
                    bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_STALL);
                    return BO_DFU_BUS_SYNCED;
                }
                if(dfu->transfer.direction_is_device_to_host)
                {
                    size_t to_send = dfu->transfer.len - (dfu->transfer.counter * BO_DFU_USB_LOW_SPEED_PACKET_SIZE);
                    if(to_send > BO_DFU_USB_LOW_SPEED_PACKET_SIZE)
                    {
                        to_send = BO_DFU_USB_LOW_SPEED_PACKET_SIZE;
                    }
                    const typeof(packet.pid_with_check) next_pid = ((dfu->transfer.counter % 2 == 0) ? BO_DFU_USB_PID_CHECK_DATA1 : BO_DFU_USB_PID_CHECK_DATA0);
                    bit_time = bo_dfu_usb_tx_data(next_pid, &dfu->transfer.data[dfu->transfer.counter * BO_DFU_USB_LOW_SPEED_PACKET_SIZE], to_send);
                    transaction_state = TRANSACTION_STATE_WAITING_ACK;
                    continue;
                }
                // Else OUT transaction; IN request; ie. Status stage. Check that the expected amount of data has been received, and send null data packet.
                if((dfu->transfer.counter * BO_DFU_USB_LOW_SPEED_PACKET_SIZE) >= dfu->transfer.len)
                {
                    bit_time = bo_dfu_usb_tx_data(BO_DFU_USB_PID_CHECK_DATA1, NULL, 0);
                    transaction_state = TRANSACTION_STATE_WAITING_ACK;
                    continue;
                }
                memset(&dfu->transfer, 0, sizeof(dfu->transfer));
                bo_dfu_update_state(dfu, ERROR, BO_DFU_STATUS_errSTALLEDPKT);
                bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_STALL);
                return BO_DFU_BUS_SYNCED;
            }
            case TRANSACTION_STATE_WAITING_ACK:
            {
                if(!bo_dfu_usb_transaction_check_ack(&packet, bytes_received))
                {
                    return BO_DFU_BUS_SYNCED;
                }
                if(!dfu->transfer.direction_is_device_to_host)
                {
                    bo_dfu_usb_transaction_complete(dfu);
                    memset(&dfu->transfer, 0, sizeof(dfu->transfer));
                    return BO_DFU_BUS_SYNCED;
                }
                ++dfu->transfer.counter;
                return BO_DFU_BUS_SYNCED;
            }
            case TRANSACTION_STATE_SETUP_WAITING_DATA:
            {
                if(!bo_dfu_usb_transaction_check_data(&packet, bytes_received))
                {
                    return BO_DFU_BUS_SYNCED;
                }
                if(bo_dfu_usb_transaction_setup(dfu, &packet, &dfu->transfer))
                {
                    bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_ACK);
                    return BO_DFU_BUS_SYNCED;
                }
                else
                {
                    memset(&dfu->transfer, 0, sizeof(dfu->transfer));
                    bo_dfu_update_state(dfu, ERROR, BO_DFU_STATUS_errSTALLEDPKT);
                    bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_STALL);
                    return BO_DFU_BUS_SYNCED;
                }
                break;
            }
            case TRANSACTION_STATE_OUT_WAITING_DATA:
            {
                if(!bo_dfu_usb_transaction_check_data(&packet, bytes_received))
                {
                    return BO_DFU_BUS_SYNCED;
                }
                // OUT
                if(!dfu->transfer.request_is_active)
                {
                    // Transaction is inactive (waiting for SETUP)
                    bo_dfu_update_state(dfu, ERROR, BO_DFU_STATUS_errSTALLEDPKT);
                    bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_STALL);
                    return BO_DFU_BUS_SYNCED;
                }
                if(dfu->transfer.direction_is_device_to_host) // IN transaction. Check this is status stage.
                {
                    if(
                        /**
                         * The host is trusted to control the flow of the request here. The device does not care if all data is actually sent or not.
                         * ie. If the host initiates the Status stage (OUT+DATA1), then all IN data is assumed to have been sent successfully.
                         * If changing this logic in future, the edge case where the last ack is lost must also be accounted for (See USB 1.1, 8.5.2.3).
                         * Do this by accepting Status stage if the last packet send has been *attempted*, even if ack was not received and thus
                         * counter will be 1 short of expected.
                        */
                        #if 0
                            dfu->transfer.counter >= ((dfu->transfer.len + (BO_DFU_USB_LOW_SPEED_PACKET_SIZE - 1)) / BO_DFU_USB_LOW_SPEED_PACKET_SIZE) &&
                        #endif
                        packet.pid_with_check == BO_DFU_USB_PID_CHECK_DATA1
                    )
                    {
                        // Done sending, Status ok. Send ACK. Request complete.
                        bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_ACK);
                        bo_dfu_usb_transaction_complete(dfu);
                        memset(&dfu->transfer, 0, sizeof(dfu->transfer));
                        return BO_DFU_BUS_SYNCED;
                    }
                    else
                    {
                        // STALL - expecting to have to send more data / invalid PID
                        memset(&dfu->transfer, 0, sizeof(dfu->transfer));
                        bo_dfu_update_state(dfu, ERROR, BO_DFU_STATUS_errSTALLEDPKT);
                        bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_STALL);
                        return BO_DFU_BUS_SYNCED;
                    }
                    break;
                }
                else
                {
                    const typeof(packet.pid_with_check) expected_pid = ((dfu->transfer.counter % 2 == 0) ? BO_DFU_USB_PID_CHECK_DATA1 : BO_DFU_USB_PID_CHECK_DATA0);
                    if(packet.pid_with_check == expected_pid)
                    {
                        const size_t data_already_received = dfu->transfer.counter * BO_DFU_USB_LOW_SPEED_PACKET_SIZE;
                        const size_t data_in_this_packet = bytes_received - 4;
                        if(
                            /**
                             * The only accepted OUT data stage in this application is for DFU DNLOAD requests.
                             * ESP32 binaries are always padded to 16 byte boundaries, so each data packet must always have either
                             * the USB Low Speed maximum (8) or 0 (for the last packet to indicate the end of the block) bytes.
                            */
                            !(
                                data_in_this_packet == 8 ||
                                (dfu->transfer.len == 0 && data_in_this_packet == 0)
                            ) ||
                            data_already_received + data_in_this_packet > dfu->transfer.len
                        )
                        {
                            bo_dfu_update_state(dfu, ERROR, BO_DFU_STATUS_errSTALLEDPKT);
                            bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_STALL);
                            return BO_DFU_BUS_SYNCED;
                        }
                        memcpy(&dfu->dfu.buffer[data_already_received], packet.data, data_in_this_packet);
                        ++dfu->transfer.counter;
                    }
                    // ACK (note: ack is sent even if PID is incorrect in order to resynchronise Data stage)
                    bo_dfu_usb_tx_handshake(BO_DFU_USB_PID_CHECK_ACK);
                    return BO_DFU_BUS_SYNCED;
                }
                break;
            }
            default:
                break;
        }
    } while(transaction_state != 0);
    return BO_DFU_BUS_SYNCED;
}

#endif /* BO_DFU_USB_TRANSFER_H */

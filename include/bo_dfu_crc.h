#ifndef BO_DFU_CRC_H
#define BO_DFU_CRC_H

#include <sys/cdefs.h>
#include <stdint.h>
#include "esp_attr.h"

/*
    CRCs are received over the wire in reverse bit order.
    These calculations therefore reverse all bit orders (including polynomials) so that results can be directly sent or compared to the
    received value without inefficient bit-reordering.
*/

#define BO_DFU_CRC5_MASK  ((1 << 5) - 1)
#define BO_DFU_CRC16_MASK ((1 << 16) - 1)
#define BO_DFU_CRC5_GENERATOR_POLYNOMIAL  (0b10100)
#define BO_DFU_CRC16_GENERATOR_POLYNOMIAL (0b1010000000000001)

static IRAM_ATTR
uint32_t bo_dfu_crc_bit(uint32_t crc, uint32_t bit_in_0, uint32_t generator_polynomial)
{
    const uint32_t do_xor = ((bit_in_0 ^ crc) & 1);
    crc >>= 1;
    if(do_xor) crc ^= generator_polynomial;
    return crc;
}

static IRAM_ATTR
uint32_t bo_dfu_usb_crc(uint32_t crc_mask, const uint8_t *data, size_t num_bits, uint32_t generator_polynomial)
{
    uint32_t crc = crc_mask;
    uint8_t this_byte;
    for(size_t i = 0; i < num_bits; ++i)
    {
        if(i % 8 == 0)
        {
            // Load next byte
            this_byte = *data;
            ++data;
        }
        crc = bo_dfu_crc_bit(crc, this_byte, generator_polynomial);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
        this_byte >>= 1;
        #pragma GCC diagnostic pop
    }
    crc ^= crc_mask;
    return crc;
}

static IRAM_ATTR
uint32_t bo_dfu_crc_token(uint16_t token)
{
    return bo_dfu_usb_crc(BO_DFU_CRC5_MASK, (const uint8_t*)&token, 11, BO_DFU_CRC5_GENERATOR_POLYNOMIAL);
}

static IRAM_ATTR
uint32_t bo_dfu_crc_data(const uint8_t *src, size_t len)
{
    return bo_dfu_usb_crc(BO_DFU_CRC16_MASK, (const uint8_t*)src, len * 8, BO_DFU_CRC16_GENERATOR_POLYNOMIAL);
}

#endif /* BO_DFU_CRC_H */

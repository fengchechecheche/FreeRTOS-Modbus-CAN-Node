#ifndef P5_MODBUS_CODEC_VECTORS_H
#define P5_MODBUS_CODEC_VECTORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
  const char *id;
  const uint8_t *frame;
  size_t length;
  bool valid_crc;
} p5_modbus_complete_frame_vector_t;

/*
 * Project Three read-only provenance:
 * HEAD 8e0e909a8b7576ab80b6f2ade186631910226b47
 * tests/data/modbus_rtu_golden_vectors.csv
 * SHA-256 995747dace392e334c9448b40071ffcee16d405d25ad731c84ece5569b6265f4
 *
 * Only complete frames are copied as oracle values. Truncated, noise-prefix
 * and concatenated stream vectors are deferred to P5-S5-T03.
 */
static const uint8_t p5_p3_g03_req_01[] =
    {0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x02U, 0xC4U, 0x0BU};
static const uint8_t p5_p3_g03_rsp_01[] =
    {0x01U, 0x03U, 0x04U, 0x00U, 0x0AU, 0x00U, 0x14U, 0xDAU, 0x3EU};
static const uint8_t p5_p3_g04_req_01[] =
    {0x01U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U, 0x31U, 0xCAU};
static const uint8_t p5_p3_g04_rsp_01[] =
    {0x01U, 0x04U, 0x02U, 0x00U, 0x0AU, 0x39U, 0x37U};
static const uint8_t p5_p3_g06_req_01[] =
    {0x01U, 0x06U, 0x00U, 0x01U, 0x00U, 0x03U, 0x98U, 0x0BU};
static const uint8_t p5_p3_g06_rsp_01[] =
    {0x01U, 0x06U, 0x00U, 0x01U, 0x00U, 0x03U, 0x98U, 0x0BU};
static const uint8_t p5_p3_g03_max_qty[] =
    {0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x7DU, 0x85U, 0xEBU};
static const uint8_t p5_p3_ex_03_02[] =
    {0x01U, 0x83U, 0x02U, 0xC0U, 0xF1U};
static const uint8_t p5_p3_ex_04_03[] =
    {0x01U, 0x84U, 0x03U, 0x03U, 0x01U};
static const uint8_t p5_p3_ex_06_04[] =
    {0x01U, 0x86U, 0x04U, 0x43U, 0xA3U};
static const uint8_t p5_p3_r_qty_zero[] =
    {0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x45U, 0xCAU};
static const uint8_t p5_p3_r_addr_overflow[] =
    {0x01U, 0x03U, 0xFFU, 0xFFU, 0x00U, 0x02U, 0xC4U, 0x2FU};
static const uint8_t p5_p3_r_broadcast[] =
    {0x00U, 0x06U, 0x00U, 0x01U, 0x00U, 0x03U, 0x99U, 0xDAU};
static const uint8_t p5_p3_r_slave_248[] =
    {0xF8U, 0x03U, 0x00U, 0x00U, 0x00U, 0x01U, 0x90U, 0x63U};
static const uint8_t p5_p3_r_bad_crc[] =
    {0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x02U, 0xC4U, 0x0AU};
static const uint8_t p5_p3_r_byte_count[] =
    {0x01U, 0x03U, 0x03U, 0x00U, 0x0AU, 0x00U, 0x43U, 0x2EU};

/* Project Five address-4 vectors independently calculated for this task. */
static const uint8_t p5_read_input_122[] =
    {0x04U, 0x04U, 0x00U, 0x00U, 0x00U, 0x7AU, 0x71U, 0xBCU};
static const uint8_t p5_read_holding_4[] =
    {0x04U, 0x03U, 0x00U, 0x00U, 0x00U, 0x04U, 0x44U, 0x5CU};
static const uint8_t p5_write_address_5[] =
    {0x04U, 0x06U, 0x00U, 0x00U, 0x00U, 0x05U, 0x49U, 0x9CU};

#define P5_FRAME_VECTOR(name, valid) \
  {#name, name, sizeof(name), valid}

static const p5_modbus_complete_frame_vector_t
    p5_modbus_complete_frame_vectors[] = {
        P5_FRAME_VECTOR(p5_p3_g03_req_01, true),
        P5_FRAME_VECTOR(p5_p3_g03_rsp_01, true),
        P5_FRAME_VECTOR(p5_p3_g04_req_01, true),
        P5_FRAME_VECTOR(p5_p3_g04_rsp_01, true),
        P5_FRAME_VECTOR(p5_p3_g06_req_01, true),
        P5_FRAME_VECTOR(p5_p3_g06_rsp_01, true),
        P5_FRAME_VECTOR(p5_p3_g03_max_qty, true),
        P5_FRAME_VECTOR(p5_p3_ex_03_02, true),
        P5_FRAME_VECTOR(p5_p3_ex_04_03, true),
        P5_FRAME_VECTOR(p5_p3_ex_06_04, true),
        P5_FRAME_VECTOR(p5_p3_r_qty_zero, true),
        P5_FRAME_VECTOR(p5_p3_r_addr_overflow, true),
        P5_FRAME_VECTOR(p5_p3_r_broadcast, true),
        P5_FRAME_VECTOR(p5_p3_r_slave_248, true),
        P5_FRAME_VECTOR(p5_p3_r_bad_crc, false),
        P5_FRAME_VECTOR(p5_p3_r_byte_count, true),
        P5_FRAME_VECTOR(p5_read_input_122, true),
        P5_FRAME_VECTOR(p5_read_holding_4, true),
        P5_FRAME_VECTOR(p5_write_address_5, true),
};

#undef P5_FRAME_VECTOR

#endif

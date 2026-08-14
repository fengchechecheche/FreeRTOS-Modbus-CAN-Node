#include "p5_modbus_crc16.h"
#include "p5_modbus_rtu_adu.h"
#include "p5_modbus_rtu_timing.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "modbus_codec_vectors.h"

#define CHECK(condition)          \
  do                              \
  {                               \
    if (!(condition))             \
    {                             \
      return EXIT_FAILURE;        \
    }                             \
  } while (false)

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static int test_crc_oracles(void)
{
  uint16_t crc = 0U;
  const uint8_t zero = 0U;
  const uint8_t digits[] =
      {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  uint8_t pattern[254];

  CHECK(p5_modbus_crc16_calculate(NULL, 0U, &crc));
  CHECK(crc == UINT16_C(0xFFFF));
  CHECK(p5_modbus_crc16_calculate(&zero, 1U, &crc));
  CHECK(crc == UINT16_C(0x40BF));
  CHECK(p5_modbus_crc16_calculate(digits, sizeof(digits), &crc));
  CHECK(crc == UINT16_C(0x4B37));

  for (size_t index = 0U; index < ARRAY_SIZE(pattern); ++index)
  {
    pattern[index] = (uint8_t)index;
  }
  CHECK(p5_modbus_crc16_calculate(pattern, sizeof(pattern), &crc));
  CHECK(crc == UINT16_C(0x576C));

  CHECK(!p5_modbus_crc16_calculate(NULL, 1U, &crc));
  CHECK(!p5_modbus_crc16_calculate(&zero, 1U, NULL));
  return EXIT_SUCCESS;
}

static int test_complete_frame_vectors(void)
{
  for (size_t index = 0U;
       index < ARRAY_SIZE(p5_modbus_complete_frame_vectors);
       ++index)
  {
    const p5_modbus_complete_frame_vector_t *vector =
        &p5_modbus_complete_frame_vectors[index];
    p5_modbus_adu_view_t view = {0U, 0U, NULL, 0U};
    const p5_modbus_adu_status_t status = p5_modbus_rtu_adu_decode(
        vector->frame, vector->length, &view);

    if (!vector->valid_crc)
    {
      CHECK(status == P5_MODBUS_ADU_CRC_MISMATCH);
      CHECK(view.data == NULL);
      CHECK(view.data_length == 0U);
      continue;
    }

    CHECK(status == P5_MODBUS_ADU_OK);
    CHECK(view.address == vector->frame[0]);
    CHECK(view.function == vector->frame[1]);
    CHECK(view.data_length == vector->length - 4U);
    CHECK(memcmp(view.data, &vector->frame[2], view.data_length) == 0);

    uint8_t encoded[P5_MODBUS_RTU_MAX_ADU_SIZE] = {0U};
    size_t encoded_length = 0U;
    CHECK(p5_modbus_rtu_adu_encode(view.address,
                                    view.function,
                                    view.data,
                                    view.data_length,
                                    encoded,
                                    sizeof(encoded),
                                    &encoded_length) == P5_MODBUS_ADU_OK);
    CHECK(encoded_length == vector->length);
    CHECK(memcmp(encoded, vector->frame, vector->length) == 0);
  }
  return EXIT_SUCCESS;
}

static int test_adu_boundaries(void)
{
  static const uint8_t minimum_expected[] =
      {0x04U, 0x04U, 0x02U, 0xB3U};
  uint8_t output[P5_MODBUS_RTU_MAX_ADU_SIZE] = {0U};
  size_t output_length = 99U;

  CHECK(p5_modbus_rtu_adu_encode(0x04U,
                                  0x04U,
                                  NULL,
                                  0U,
                                  output,
                                  sizeof(output),
                                  &output_length) == P5_MODBUS_ADU_OK);
  CHECK(output_length == sizeof(minimum_expected));
  CHECK(memcmp(output, minimum_expected, sizeof(minimum_expected)) == 0);

  uint8_t maximum_data[P5_MODBUS_RTU_MAX_FUNCTION_DATA_SIZE];
  for (size_t index = 0U; index < ARRAY_SIZE(maximum_data); ++index)
  {
    maximum_data[index] = (uint8_t)index;
  }
  CHECK(p5_modbus_rtu_adu_encode(0x04U,
                                  0x04U,
                                  maximum_data,
                                  sizeof(maximum_data),
                                  output,
                                  sizeof(output),
                                  &output_length) == P5_MODBUS_ADU_OK);
  CHECK(output_length == P5_MODBUS_RTU_MAX_ADU_SIZE);

  p5_modbus_adu_view_t view = {0U, 0U, NULL, 0U};
  CHECK(p5_modbus_rtu_adu_decode(output, output_length, &view) ==
        P5_MODBUS_ADU_OK);
  CHECK(view.data_length == sizeof(maximum_data));
  CHECK(memcmp(view.data, maximum_data, sizeof(maximum_data)) == 0);

  output_length = 99U;
  CHECK(p5_modbus_rtu_adu_encode(0x04U,
                                  0x04U,
                                  maximum_data,
                                  sizeof(maximum_data),
                                  output,
                                  sizeof(output) - 1U,
                                  &output_length) ==
        P5_MODBUS_ADU_OUTPUT_TOO_SMALL);
  CHECK(output_length == 0U);

  uint8_t too_much_data[P5_MODBUS_RTU_MAX_FUNCTION_DATA_SIZE + 1U] = {0U};
  output_length = 99U;
  CHECK(p5_modbus_rtu_adu_encode(0x04U,
                                  0x04U,
                                  too_much_data,
                                  sizeof(too_much_data),
                                  output,
                                  sizeof(output),
                                  &output_length) == P5_MODBUS_ADU_DATA_TOO_LARGE);
  CHECK(output_length == 0U);

  output_length = 99U;
  CHECK(p5_modbus_rtu_adu_encode(0x04U,
                                  0x04U,
                                  NULL,
                                  1U,
                                  output,
                                  sizeof(output),
                                  &output_length) ==
        P5_MODBUS_ADU_INVALID_ARGUMENT);
  CHECK(output_length == 0U);
  CHECK(p5_modbus_rtu_adu_encode(0x04U,
                                  0x04U,
                                  NULL,
                                  0U,
                                  NULL,
                                  0U,
                                  &output_length) ==
        P5_MODBUS_ADU_INVALID_ARGUMENT);
  CHECK(p5_modbus_rtu_adu_encode(0x04U,
                                  0x04U,
                                  NULL,
                                  0U,
                                  output,
                                  sizeof(output),
                                  NULL) == P5_MODBUS_ADU_INVALID_ARGUMENT);

  uint8_t overlong[P5_MODBUS_RTU_MAX_ADU_SIZE + 1U] = {0U};
  CHECK(p5_modbus_rtu_adu_decode(overlong, sizeof(overlong), &view) ==
        P5_MODBUS_ADU_FRAME_TOO_LONG);
  CHECK(view.data == NULL);
  CHECK(p5_modbus_rtu_adu_decode(output, 3U, &view) ==
        P5_MODBUS_ADU_FRAME_TOO_SHORT);
  CHECK(view.data == NULL);
  CHECK(p5_modbus_rtu_adu_decode(NULL, 0U, &view) ==
        P5_MODBUS_ADU_INVALID_ARGUMENT);
  CHECK(p5_modbus_rtu_adu_decode(output, 4U, NULL) ==
        P5_MODBUS_ADU_INVALID_ARGUMENT);
  return EXIT_SUCCESS;
}

static int test_corruption_and_wire_order(void)
{
  uint8_t corrupted[sizeof(p5_read_input_122)];
  for (size_t index = 0U; index < sizeof(corrupted); ++index)
  {
    (void)memcpy(corrupted, p5_read_input_122, sizeof(corrupted));
    corrupted[index] ^= UINT8_C(0x01);
    p5_modbus_adu_view_t view = {0U, 0U, NULL, 0U};
    CHECK(p5_modbus_rtu_adu_decode(corrupted, sizeof(corrupted), &view) ==
          P5_MODBUS_ADU_CRC_MISMATCH);
    CHECK(view.data == NULL);
  }

  (void)memcpy(corrupted, p5_read_input_122, sizeof(corrupted));
  const uint8_t low = corrupted[sizeof(corrupted) - 2U];
  corrupted[sizeof(corrupted) - 2U] = corrupted[sizeof(corrupted) - 1U];
  corrupted[sizeof(corrupted) - 1U] = low;
  p5_modbus_adu_view_t view = {0U, 0U, NULL, 0U};
  CHECK(p5_modbus_rtu_adu_decode(corrupted, sizeof(corrupted), &view) ==
        P5_MODBUS_ADU_CRC_MISMATCH);
  return EXIT_SUCCESS;
}

static int test_timing(void)
{
  typedef struct
  {
    uint32_t baud;
    uint32_t character_us;
    uint32_t inter_character_us;
    uint32_t inter_frame_us;
  } timing_vector_t;

  static const timing_vector_t vectors[] = {
      {9600U, 1146U, 1719U, 4011U},
      {19200U, 573U, 860U, 2006U},
      {19201U, 573U, 750U, 1750U},
      {38400U, 287U, 750U, 1750U},
      {115200U, 96U, 750U, 1750U},
  };

  for (size_t index = 0U; index < ARRAY_SIZE(vectors); ++index)
  {
    p5_modbus_rtu_timing_t timing = {0U, 0U, 0U};
    CHECK(p5_modbus_rtu_timing_8e1(vectors[index].baud, &timing));
    CHECK(timing.character_us == vectors[index].character_us);
    CHECK(timing.inter_character_us == vectors[index].inter_character_us);
    CHECK(timing.inter_frame_us == vectors[index].inter_frame_us);
  }

  p5_modbus_rtu_timing_t unchanged = {1U, 2U, 3U};
  CHECK(!p5_modbus_rtu_timing_8e1(0U, &unchanged));
  CHECK(unchanged.character_us == 1U);
  CHECK(unchanged.inter_character_us == 2U);
  CHECK(unchanged.inter_frame_us == 3U);
  CHECK(!p5_modbus_rtu_timing_8e1(19200U, NULL));
  return EXIT_SUCCESS;
}

int main(void)
{
  CHECK(test_crc_oracles() == EXIT_SUCCESS);
  CHECK(test_complete_frame_vectors() == EXIT_SUCCESS);
  CHECK(test_adu_boundaries() == EXIT_SUCCESS);
  CHECK(test_corruption_and_wire_order() == EXIT_SUCCESS);
  CHECK(test_timing() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}

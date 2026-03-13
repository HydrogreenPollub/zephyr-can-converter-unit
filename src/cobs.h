#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Worst-case encoded length for a payload of N bytes.
 * Use this to size the encode output buffer.
 */
#define COBS_ENCODED_MAX_SIZE(n) ((n) + ((n) / 254u) + 1u)

/**
 * Encode a buffer with COBS.  The result does not include the 0x00 frame
 * delimiters — the caller wraps the output in [0x00 ... 0x00] when transmitting.
 *
 * @param data         Payload to encode.
 * @param data_len     Length of the payload.
 * @param encoded      Output buffer (must hold at least COBS_ENCODED_MAX_SIZE(data_len) bytes).
 * @param encoded_max  Capacity of the output buffer.
 * @return             Number of encoded bytes written, or 0 on any error.
 */
size_t cobs_encode(const uint8_t *data, size_t data_len,
                   uint8_t *encoded, size_t encoded_max);

/**
 * Decode a COBS-encoded buffer (without the surrounding 0x00 frame delimiters).
 *
 * @param encoded      Input buffer (raw bytes between two 0x00 delimiters).
 * @param encoded_len  Number of bytes in the input buffer.
 * @param decoded      Output buffer for the recovered payload.
 * @param decoded_max  Capacity of the output buffer.
 * @return             Number of decoded bytes written, or 0 on any error.
 */
size_t cobs_decode(const uint8_t *encoded, size_t encoded_len,
                   uint8_t *decoded, size_t decoded_max);

#ifdef __cplusplus
}
#endif

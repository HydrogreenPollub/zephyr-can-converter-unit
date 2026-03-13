#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Worst-case encoded length for a payload of @p n bytes.
 *
 * Use this macro to stack-allocate the output buffer for cobs_encode():
 * @code
 *   uint8_t out[COBS_ENCODED_MAX_SIZE(payload_len)];
 * @endcode
 */
#define COBS_ENCODED_MAX_SIZE(n) ((n) + ((n) / 254u) + 1u)

/**
 * @brief Encode a buffer with COBS.
 *
 * Transforms @p data so that it contains no 0x00 bytes.  The result does
 * NOT include the surrounding 0x00 frame delimiters — the caller is
 * responsible for emitting those on the wire.
 *
 * @param data         Payload to encode.
 * @param data_len     Number of bytes in @p data.
 * @param encoded      Output buffer.  Must be at least
 *                     COBS_ENCODED_MAX_SIZE(@p data_len) bytes.
 * @param encoded_max  Capacity of @p encoded in bytes.
 *
 * @return Number of bytes written to @p encoded, or 0 on error
 *         (NULL pointer, zero length, or output buffer too small).
 */
size_t cobs_encode(const uint8_t *data, size_t data_len,
                   uint8_t *encoded, size_t encoded_max);

/**
 * @brief Decode a COBS-encoded buffer.
 *
 * Recovers the original payload from the bytes that were received between
 * two 0x00 frame delimiters.  The delimiters themselves must NOT be included
 * in @p encoded.
 *
 * @param encoded      COBS-encoded input (the raw bytes between two 0x00s).
 * @param encoded_len  Number of bytes in @p encoded.
 * @param decoded      Output buffer for the recovered payload.
 * @param decoded_max  Capacity of @p decoded in bytes.
 *
 * @return Number of bytes written to @p decoded, or 0 on error
 *         (NULL pointer, zero length, output buffer too small, or malformed
 *         input — e.g. a 0x00 byte appearing inside the encoded stream).
 */
size_t cobs_decode(const uint8_t *encoded, size_t encoded_len,
                   uint8_t *decoded, size_t decoded_max);

#ifdef __cplusplus
}
#endif

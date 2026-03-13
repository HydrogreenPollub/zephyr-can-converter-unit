#include "cobs.h"

size_t cobs_encode(const uint8_t *data, size_t data_len,
                   uint8_t *encoded, size_t encoded_max) {
    if (!data || !encoded || data_len == 0) {
        return 0;
    }

    size_t read_idx = 0;
    size_t write_idx = 0;
    size_t code_idx;
    uint8_t code = 1u;

    /* Reserve slot for the first code byte */
    if (write_idx >= encoded_max) {
        return 0;
    }
    code_idx = write_idx++;

    while (read_idx < data_len) {
        if (data[read_idx] == 0x00u) {
            /* End of block: commit code, start a new block */
            encoded[code_idx] = code;
            code = 1u;
            if (write_idx >= encoded_max) {
                return 0;
            }
            code_idx = write_idx++;
        } else {
            if (write_idx >= encoded_max) {
                return 0;
            }
            encoded[write_idx++] = data[read_idx];
            code++;

            if (code == 0xFFu) {
                /* Block full (254 non-zero bytes): commit, start new block.
                 * No stuffed zero is inserted for 0xFF blocks. */
                encoded[code_idx] = code;
                code = 1u;
                if (read_idx + 1u < data_len) {
                    if (write_idx >= encoded_max) {
                        return 0;
                    }
                    code_idx = write_idx++;
                } else {
                    /* Last byte completed a full 254-byte run; no trailing
                     * code byte needed. */
                    return write_idx;
                }
            }
        }
        read_idx++;
    }

    /* Commit the final code byte */
    encoded[code_idx] = code;
    return write_idx;
}

size_t cobs_decode(const uint8_t *encoded, size_t encoded_len,
                   uint8_t *decoded, size_t decoded_max) {
    if (!encoded || !decoded || encoded_len == 0) {
        return 0;
    }

    size_t read_idx = 0;
    size_t write_idx = 0;

    while (read_idx < encoded_len) {
        uint8_t code = encoded[read_idx++];
        if (code == 0x00u) {
            return 0; /* 0x00 is not valid inside a COBS payload */
        }

        uint8_t block_len = code - 1u;

        for (uint8_t i = 0; i < block_len; i++) {
            if (read_idx >= encoded_len || write_idx >= decoded_max) {
                return 0;
            }
            decoded[write_idx++] = encoded[read_idx++];
        }

        /*
         * After each block, COBS implicitly inserts a 0x00 back – except:
         *   - code == 0xFF: 254 consecutive non-zero bytes, no stuffed zero
         *   - last block: the original data did not end with a 0x00
         */
        if (code != 0xFFu && read_idx < encoded_len) {
            if (write_idx >= decoded_max) {
                return 0;
            }
            decoded[write_idx++] = 0x00u;
        }
    }

    return write_idx;
}

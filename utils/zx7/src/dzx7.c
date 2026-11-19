/*
 * (c) Copyright 2015 by Einar Saukas. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of its author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "zx7.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const unsigned char *input_data;
    unsigned char *output_data;
    unsigned int input_index;
    unsigned int input_size;
    unsigned int output_index;
    unsigned int output_size;
    int bit_mask;
    int bit_value;
    int error;
} ZX7State;

static inline int d_read_byte(ZX7State *s) {
    if (s->input_index >= s->input_size) {
        s->error = 1;
        return 0;
    }
    return s->input_data[s->input_index++];
}

static inline int d_read_bit(ZX7State *s) {
    s->bit_mask >>= 1;
    if (s->bit_mask == 0) {
        s->bit_mask = 128;
        s->bit_value = d_read_byte(s);
    }
    return (s->bit_value & s->bit_mask) ? 1 : 0;
}

static inline int read_elias_gamma(ZX7State *s) {
    int i = 0;
    int value;

    while (!d_read_bit(s)) {
        i++;
        if (s->error || i > 16) {
            s->error = 1;
            return -2;
        }
    }
    if (s->error) {
        return -2;
    }
    if (i == 16) {
        return -1; /* EOF marker */
    }
    value = 1;
    while (i--) {
        value = (value << 1) | d_read_bit(s);
        if (s->error) {
            return -2;
        }
    }
    return value;
}

static int read_offset(ZX7State *s) {
    int value = d_read_byte(s);
    if (s->error) {
        return -1;
    }
    if (value < 128) {
        return value;
    } else {
        int i = d_read_bit(s);
        i = (i << 1) | d_read_bit(s);
        i = (i << 1) | d_read_bit(s);
        i = (i << 1) | d_read_bit(s);
        if (s->error) {
            return -1;
        }
        return ((value & 127) | (i << 7)) + 128;
    }
}

static inline int d_write_byte(ZX7State *s, int value) {
    if (s->output_index >= s->output_size) {
        s->error = 1;
        return 0;
    }
    s->output_data[s->output_index++] = (unsigned char)value;
    return 1;
}

static int d_write_bytes(ZX7State *s, unsigned int offset, unsigned int length) {
    if (offset == 0 || offset > s->output_index || length > s->output_size - s->output_index) {
        s->error = 1;
        return 0;
    }
    while (length-- > 0) {
        unsigned char b = s->output_data[s->output_index - offset];
        s->output_data[s->output_index++] = b;
    }
    return 1;
}

unsigned int ZX7GetDecompressedSize(const unsigned char* compressedData) {
    if (!compressedData) {
        return 0;
    }
    return ((unsigned int)compressedData[0] << 16) |
           ((unsigned int)compressedData[1] << 8) |
           (unsigned int)compressedData[2];
}

int ZX7Decompress(const unsigned char* srcData, unsigned int srcLength, unsigned char* destData, unsigned int destLength) {
    ZX7State s;
    unsigned int expectedSize;
    int firstByte;

    if (!srcData || !destData || srcLength < 4) {
        return -1;
    }

    expectedSize = ZX7GetDecompressedSize(srcData);
    if (expectedSize == 0 || expectedSize > destLength) {
        return -1;
    }

    s.input_data = srcData + 3;
    s.output_data = destData;
    s.input_index = 0;
    s.input_size = srcLength - 3;
    s.output_index = 0;
    s.output_size = expectedSize;
    s.bit_mask = 0;
    s.bit_value = 0;
    s.error = 0;

    firstByte = d_read_byte(&s);
    if (s.error || !d_write_byte(&s, firstByte)) {
        return -1;
    }

    while (1) {
        int bit = d_read_bit(&s);
        if (s.error) {
            return -1;
        }
        if (!bit) {
            int lit = d_read_byte(&s);
            if (s.error || !d_write_byte(&s, lit)) {
                return -1;
            }
        } else {
            int gamma = read_elias_gamma(&s);
            int offset;
            if (gamma == -1) {
                return (!s.error && s.output_index == expectedSize) ? 0 : -1;
            }
            if (gamma < 0) {
                return -1;
            }
            offset = read_offset(&s);
            if (s.error || offset < 0) {
                return -1;
            }
            if (!d_write_bytes(&s, (unsigned int)offset + 1U, (unsigned int)gamma + 1U)) {
                return -1;
            }
        }
    }
}

#ifdef __cplusplus
}
#endif

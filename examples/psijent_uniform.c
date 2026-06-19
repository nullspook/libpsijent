/*
 * Copyright (C) 2026 NullSpook
 *
 * This file is part of libpsijent.
 *
 * libpsijent is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * psijent is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with psijent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "psijent.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void randuniform(psijent* p, float* dest, const int length)
{
    static const int precision_bits = 23;

    enum { buffer_size = 4096 };
    static const int max_chunk_size_bytes = buffer_size / 8;

    uint8_t buffer[buffer_size];

    float inv = ldexpf(1.0f, -precision_bits);

    int remaining = length;
    int offset = 0;

    while (remaining > 0) {
        int chunk = remaining < max_chunk_size_bytes ? remaining : max_chunk_size_bytes;

        psijent_randbytes(
            p,
            buffer,
            (chunk * precision_bits + 7) / 8,
            false,  // decorrelate_with_lfsr
            true,   // mask_with_prng
            0       // bit_bias_amplification_level
        );

        size_t bit_offset = 0;
        for (int i = 0; i < chunk; ++i) {
            uint64_t acc = 0;
            for (int j = 0; j < precision_bits; ++j) {
                acc |= (uint64_t)((buffer[bit_offset / 8] >> (bit_offset % 8)) & 1) << j;
                ++bit_offset;
            }
            dest[offset + i] = (float)acc * inv;
        }

        offset += chunk;
        remaining -= chunk;
    }
}

int main(int argc, char* argv[])
{
    psijent* p = NULL;
    int result;
    int count = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s COUNT\n", argv[0]);
        return EXIT_FAILURE;
    }

    count = atoi(argv[1]);

    if ((result = psijent_init(&p)) != 0) {
        fprintf(stderr, "psijent_init failed: %d\n", result);
        return EXIT_FAILURE;
    }

    float* values = malloc((size_t)count * sizeof(float));
    if (values == NULL) {
        fprintf(stderr, "malloc failed\n");
        psijent_free(p);
        return EXIT_FAILURE;
    }

    randuniform(p, values, count);

    for (int i = 0; i < count; ++i) {
        printf("%.9f\n", values[i]);
    }

    free(values);
    psijent_free(p);

    return EXIT_SUCCESS;
}

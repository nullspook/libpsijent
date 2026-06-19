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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct psijent psijent;

int psijent_init(psijent** p);

void psijent_free(psijent* p);

void psijent_raw_deltas(psijent* p, uint64_t* dest, int length);

double psijent_estimate_min_entropy_per_bit(psijent* p, int sample_size);

void psijent_randbits_unpacked(
    psijent* p,
    uint8_t* dest,
    int length,
    bool decorrelate_with_lfsr,
    bool mask_with_prng,
    int bias_amplification_level
);

void psijent_randbytes(
    psijent* p,
    uint8_t* dest,
    int length,
    bool decorrelate_with_lfsr,
    bool mask_with_prng,
    int bit_bias_amplification_level
);

#ifdef __cplusplus
}
#endif

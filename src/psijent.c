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

#include "hwtimestamp.h"

#if !defined(_MSC_VER)
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#else
#include <windows.h>
#endif

#define MEM_SIZE (1 << 26) // 64 MiB
#define MEM_LOC_MASK (MEM_SIZE - 1)
#define MEM_MIN_JUMP_DIST (1 << 24) // 16 MiB
#define MEM_RANDOMIZED_JUMP_MASK ((1 << 24) - 1)

#define GOLDEN 0x9E3779B97F4A7C15

struct psijent {
    uint8_t* mem;
    uint32_t mem_read_location;
    uint32_t mem_write_location;
    uint32_t randomized_jump_state;
    uint64_t decorrelator_lfsr_state;
    uint64_t mask_prng_state;
};

static uint64_t measure_delta(
    uint8_t* mem,
    uint32_t* mem_read_location,
    uint32_t* mem_write_location,
    uint32_t* randomized_jump_state
) {
    static const int loop_count = 4096;

    uint64_t hw_timestamp_start, hw_timestamp_end;
    uint32_t aux;

    read_hw_timestamp_start(&hw_timestamp_start);

    for (int i = 0; i < loop_count; ++i) {
        *randomized_jump_state ^= *randomized_jump_state << 13;
        *randomized_jump_state ^= *randomized_jump_state >> 17;
        *randomized_jump_state ^= *randomized_jump_state << 5;
        *mem_write_location = (*mem_read_location + MEM_MIN_JUMP_DIST + (*randomized_jump_state & MEM_RANDOMIZED_JUMP_MASK)) & MEM_LOC_MASK;

        *randomized_jump_state ^= *randomized_jump_state << 13;
        *randomized_jump_state ^= *randomized_jump_state >> 17;
        *randomized_jump_state ^= *randomized_jump_state << 5;
        *mem_read_location = (*mem_write_location + MEM_MIN_JUMP_DIST + (*randomized_jump_state & MEM_RANDOMIZED_JUMP_MASK)) & MEM_LOC_MASK;

        mem[*mem_write_location] = mem[*mem_read_location] + 1;
    }

    read_hw_timestamp_end(&hw_timestamp_end, &aux);

    const uint64_t delta = hw_timestamp_end - hw_timestamp_start;

    *randomized_jump_state += delta;

    if (*randomized_jump_state == 0) {
        *randomized_jump_state = GOLDEN & 0xFFFFFFFF;
    }

    return delta;
}

static uint8_t next_entropic_bit(psijent* p)
{
    return measure_delta(
        p->mem,
        &p->mem_read_location,
        &p->mem_write_location,
        &p->randomized_jump_state
    ) & 1;
}

static uint8_t next_mask_bit(psijent* p)
{
    uint64_t oldstate = p->mask_prng_state;
    p->mask_prng_state = oldstate * 6364136223846793005ull + 1;
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return ((xorshifted >> rot) | (xorshifted << ((-rot) & 31))) & 1;
}

static uint8_t decorrelate(psijent* p, const uint8_t input_bit)
{
    // https://poincare.matf.bg.ac.rs/~ezivkovm/publications/primpol1.pdf
    static const int taps[4] = { 63, 60, 33, 8 };

    if (p->decorrelator_lfsr_state == 0) {
        p->decorrelator_lfsr_state = GOLDEN;
    }

    uint64_t feedback_bit = input_bit;

    for (int i = 0; i < 4; ++i) {
        feedback_bit ^= (p->decorrelator_lfsr_state >> taps[i]) & 1;
    }

    p->decorrelator_lfsr_state = (p->decorrelator_lfsr_state << 1) | feedback_bit;

    return feedback_bit;
}

double psijent_estimate_min_entropy_per_bit(psijent* p, const int sample_size)
{
    int ones = 0;
    for (int i = 0; i < sample_size; ++i) {
        ones += next_entropic_bit(p);
    }
    const double p_ones = (double)ones / sample_size;
    const double p_zeros = 1.0 - p_ones;
    const double max_p = p_ones > p_zeros ? p_ones : p_zeros;
    return -log2(max_p);
}

int psijent_init(psijent** p)
{
    *p = (psijent*)malloc(sizeof(psijent));
    if (!*p) {
        return -1;
    }

#if !defined(_MSC_VER)
    (*p)->mem = (uint8_t*)mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((*p)->mem == MAP_FAILED) {
        free(*p);
        *p = NULL;
        return -1;
    }
#else
    (*p)->mem = (uint8_t*)VirtualAlloc(NULL, MEM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!(*p)->mem) {
        free(*p);
        *p = NULL;
        return -1;
    }
#endif

    memset((*p)->mem, GOLDEN & 0xFF, MEM_SIZE);

    (*p)->mem_read_location = 0;
    (*p)->mem_write_location = 0;
    (*p)->randomized_jump_state = GOLDEN & 0xFFFFFFFF;
    (*p)->decorrelator_lfsr_state = GOLDEN;
    (*p)->mask_prng_state = GOLDEN;

    static const int estimation_sample_size = 1024;
    if (psijent_estimate_min_entropy_per_bit(*p, estimation_sample_size) < 0.8) {
        psijent_free(*p);
        *p = NULL;
        return -1;
    }

    return 0;
}

void psijent_free(psijent* p)
{
    if (!p) {
        return;
    }

    if (p->mem) {
#if !defined(_MSC_VER)
        munmap(p->mem, MEM_SIZE);
#else
        VirtualFree(p->mem, 0, MEM_RELEASE);
#endif
        p->mem = NULL;
    }

    free(p);
}

void psijent_raw_deltas(psijent* p, uint64_t* dest, const int length)
{
    for (int i = 0; i < length; ++i) {
        dest[i] = measure_delta(
            p->mem,
            &p->mem_read_location,
            &p->mem_write_location,
            &p->randomized_jump_state
        );
    }
}

void psijent_randbits_unpacked(
    psijent* p,
    uint8_t* dest,
    const int length,
    const bool decorrelate_with_lfsr,
    const bool mask_with_prng,
    const int bias_amplification_level
) {
    // Level 0: bound = 1 (no amplification)
    // Level 1: bound = 2 (reverse von Neumann)
    // Level 2: bound = 3
    // Level 3: bound = 4
    // ...
    const int32_t bound = bias_amplification_level + 1;

    for (int i = 0; i < length; ++i) {
        int32_t pos = 0;

        while (true) {
            uint8_t bit = decorrelate_with_lfsr
                  ? decorrelate(p, next_entropic_bit(p))
                  : next_entropic_bit(p);

            if (mask_with_prng) {
                bit ^= next_mask_bit(p);
            }

            pos += bit * 2 - 1;

            if (pos == bound || pos == -bound) {
                break;
            }
        }

        dest[i] = (uint8_t)((pos >> 31) + 1);
    }
}

void psijent_randbytes(
    psijent* p,
    uint8_t* dest,
    const int length,
    const bool decorrelate_with_lfsr,
    const bool mask_with_prng,
    const int bit_bias_amplification_level
) {
    enum { bit_buffer_size = 32768 };
    static const int max_chunk_size_bytes = bit_buffer_size / 8;

    uint8_t bit_buffer[bit_buffer_size];

    int remaining = length;
    int offset = 0;

    while (remaining > 0) {
        const int chunk = remaining < max_chunk_size_bytes ? remaining : max_chunk_size_bytes;

        psijent_randbits_unpacked(
            p,
            bit_buffer,
            chunk * 8,
            decorrelate_with_lfsr,
            mask_with_prng,
            bit_bias_amplification_level
        );

        int base = 0;
        for (int i = 0; i < chunk; ++i) {
            dest[offset + i] =
                    bit_buffer[base] |
                    (bit_buffer[base + 1] << 1) |
                    (bit_buffer[base + 2] << 2) |
                    (bit_buffer[base + 3] << 3) |
                    (bit_buffer[base + 4] << 4) |
                    (bit_buffer[base + 5] << 5) |
                    (bit_buffer[base + 6] << 6) |
                    (bit_buffer[base + 7] << 7);
            base += 8;
        }

        offset += chunk;
        remaining -= chunk;
    }
}

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

#include <cstdint>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

/*
 * References:
 * - https://github.com/abseil/abseil-cpp/blob/889ddc99e10c02c7110223e6d0dc41d83f0c0b60/absl/random/internal/nanobenchmark.cc
 * - https://github.com/cloudius-systems/osv/blob/f8caf9d127e7275d5c72116b887fa8d74e9d3929/arch/aarch64/arm-clock.cc
 */

inline
#if defined(_MSC_VER)
__forceinline
#else
__attribute__((always_inline))
#endif
void read_hw_timestamp_start(uint64_t& out)
{
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__amd64__))
#if !defined(__clang_major__) || __clang_major__ >= 11
    asm volatile inline(
#else
    asm volatile(
#endif
        "lfence\n\t"
        "rdtsc\n\t"
        "shl $32, %%rdx\n\t"
        "or %%rdx, %0\n\t"
        "lfence"
        : "=a"(out)
        :
        : "rdx", "memory", "cc");
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
#if !defined(__clang_major__) || __clang_major__ >= 11
    asm volatile inline(
#else
    asm volatile(
#endif
        "isb\n\t"
        "mrs %0, cntvct_el0"
        : "=r"(out)
        :
        : "memory");
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    _ReadWriteBarrier();
    _mm_lfence();
    _ReadWriteBarrier();
    out = __rdtsc();
    _ReadWriteBarrier();
    _mm_lfence();
    _ReadWriteBarrier();
#else
#error Unsupported architecture or compiler
#endif
}

inline
#if defined(_MSC_VER)
__forceinline
#else
__attribute__((always_inline))
#endif
void read_hw_timestamp_end(uint64_t& out, uint32_t& aux)
{
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__amd64__))
#if !defined(__clang_major__) || __clang_major__ >= 11
    asm volatile inline(
#else
    asm volatile(
#endif
        "rdtscp\n\t"
        "shl $32, %%rdx\n\t"
        "or %%rdx, %0\n\t"
        "lfence"
        : "=a"(out)
        :
        : "rcx", "rdx", "memory", "cc");
    aux = 0;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
#if !defined(__clang_major__) || __clang_major__ >= 11
    asm volatile inline(
#else
    asm volatile(
#endif
        "isb\n\t"
        "mrs %0, cntvct_el0"
        : "=r"(out)
        :
        : "memory");
    aux = 0;
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    _ReadWriteBarrier();
    out = __rdtscp(&aux);
    _ReadWriteBarrier();
    _mm_lfence();
    _ReadWriteBarrier();
#else
#error Unsupported architecture or compiler
#endif
}

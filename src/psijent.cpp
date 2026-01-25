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

#ifdef _MSC_VER
#define NOMINMAX
#include <windows.h>
#endif

#include "psijent.h"

#include "hwtimestamp.h"

#include <algorithm>
#include <bit>
#include <mutex>
#include <thread>

#ifdef _MSC_VER
#include <synchapi.h>
#else
#include <semaphore>
#endif

using namespace std::chrono;

static void throttle()
{
#ifdef _MSC_VER
    SwitchToThread();
#else
    constexpr timespec ts { 0, 1 };
#ifdef __APPLE__
    nanosleep(&ts, nullptr);
#else
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, nullptr);
#endif
#endif
}

struct psijent {
    uint8_t* bit_buffer;
    int bit_buffer_length;
    int bit_buffer_idx;

#ifdef _MSC_VER
    HANDLE bit_buffer_filled_event;
    HANDLE start_done_event;
    HANDLE stop_done_event;
#else
    std::binary_semaphore bit_buffer_filled_sem{0};
    std::binary_semaphore start_done_sem{0};
    std::binary_semaphore stop_done_sem{0};
#endif

    bool running;

    std::mutex randbits_mtx;
};

int psijent_init(psijent** p)
{
    uint64_t hw_timestamp_start, hw_timestamp_end;
    uint32_t aux;
    read_hw_timestamp_start(hw_timestamp_start);
    std::this_thread::sleep_for(milliseconds(1));
    read_hw_timestamp_end(hw_timestamp_end, aux);
    if (hw_timestamp_end - hw_timestamp_start <= 0) {
        return PSIJENT_RESULT_HW_TIMEKEEPER_UNSUPPORTED;
    }

    *p = new psijent{};
    if (!*p) {
        return PSIJENT_RESULT_ERROR_UNEXPECTED;
    }

#ifdef _MSC_VER
    (*p)->bit_buffer_filled_event = CreateEvent(nullptr, true, false, nullptr);
    if (!(*p)->bit_buffer_filled_event) {
        delete *p;
        *p = nullptr;
        return PSIJENT_RESULT_ERROR_UNEXPECTED;
    }
    (*p)->start_done_event = CreateEvent(nullptr, true, false, nullptr);
    if (!(*p)->start_done_event) {
        delete *p;
        *p = nullptr;
        return PSIJENT_RESULT_ERROR_UNEXPECTED;
    }
    (*p)->stop_done_event = CreateEvent(nullptr, true, false, nullptr);
    if (!(*p)->stop_done_event) {
        delete *p;
        *p = nullptr;
        return PSIJENT_RESULT_ERROR_UNEXPECTED;
    }
#endif

    (*p)->bit_buffer = nullptr;
    (*p)->bit_buffer_length = 0;
    (*p)->bit_buffer_idx = 0;

    (*p)->running = false;

    return PSIJENT_RESULT_OK;
}

void psijent_free(psijent* p)
{
    if (p->running) {
        psijent_stop(p);
    }

    p->bit_buffer = nullptr;

#ifdef _MSC_VER
    if (p->bit_buffer_filled_event) {
        CloseHandle(p->bit_buffer_filled_event);
    }
    if (p->start_done_event) {
        CloseHandle(p->start_done_event);
    }
    if (p->stop_done_event) {
        CloseHandle(p->stop_done_event);
    }
#endif

    delete p;
}

#if defined(_MSC_VER)
DWORD WINAPI background_thread_func(LPVOID param)
{
    auto p = static_cast<psijent*>(param);
#else
static void background_thread_func(psijent* p)
{
#endif
    const auto mem = static_cast<uint8_t*>(malloc(PSIJENT_MEM_SIZE));
    if (!mem) {
        throw std::bad_alloc();
    }

    uint32_t mem_location = 0;
    uint32_t randomized_jump_state = 1;

    uint64_t hw_timestamp_start, hw_timestamp_end, hw_timestamp_delta;
    uint32_t aux;
    int i;

    uint64_t warmup_cycles_remaining = PSIJENT_MIN_WARMUP_CYCLES;
    const auto warmup_start_time = steady_clock::now();
    while (steady_clock::now() - warmup_start_time < milliseconds(PSIJENT_MIN_WARMUP_DURATION_MILLIS) || warmup_cycles_remaining > 0) {
#if PSIJENT_THROTTLE
        throttle();
#endif
        read_hw_timestamp_start(hw_timestamp_start);
        for (i = 0; i < PSIJENT_MEM_LOOP_COUNT; ++i) {
            randomized_jump_state ^= randomized_jump_state << 13;
            randomized_jump_state ^= randomized_jump_state >> 17;
            randomized_jump_state ^= randomized_jump_state << 5;
            ++mem[mem_location = (mem_location + PSIJENT_MEM_MIN_JUMP_DIST + (randomized_jump_state & PSIJENT_MEM_RANDOMIZED_JUMP_MASK)) & PSIJENT_MEM_LOC_MASK];
        }
        read_hw_timestamp_end(hw_timestamp_end, aux);
        hw_timestamp_delta = hw_timestamp_end - hw_timestamp_start;
        randomized_jump_state = randomized_jump_state + hw_timestamp_delta;
        if (randomized_jump_state == 0) {
            randomized_jump_state = 1;
        }
        if (warmup_cycles_remaining > 0) {
            --warmup_cycles_remaining;
        }
    }

#ifdef _MSC_VER
    SetEvent(p->start_done_event);
#else
    p->start_done_sem.release();
#endif

    while (p->running) {
#if PSIJENT_THROTTLE
        throttle();
#endif
        read_hw_timestamp_start(hw_timestamp_start);
        for (i = 0; i < PSIJENT_MEM_LOOP_COUNT; ++i) {
            randomized_jump_state ^= randomized_jump_state << 13;
            randomized_jump_state ^= randomized_jump_state >> 17;
            randomized_jump_state ^= randomized_jump_state << 5;
            ++mem[mem_location = (mem_location + PSIJENT_MEM_MIN_JUMP_DIST + (randomized_jump_state & PSIJENT_MEM_RANDOMIZED_JUMP_MASK)) & PSIJENT_MEM_LOC_MASK];
        }
        read_hw_timestamp_end(hw_timestamp_end, aux);
        hw_timestamp_delta = hw_timestamp_end - hw_timestamp_start;
        randomized_jump_state = randomized_jump_state + hw_timestamp_delta;
        if (randomized_jump_state == 0) {
            randomized_jump_state = 1;
        }
        if (p->bit_buffer) {
            p->bit_buffer[p->bit_buffer_idx++] = static_cast<uint8_t>(hw_timestamp_delta & 1);
            if (p->bit_buffer_idx == p->bit_buffer_length) {
                p->bit_buffer = nullptr;
#ifdef _MSC_VER
                SetEvent(p->bit_buffer_filled_event);
#else
                p->bit_buffer_filled_sem.release();
#endif
            }
        }
    }

    free(mem);

#ifdef _MSC_VER
    SetEvent(p->bit_buffer_filled_event);
    SetEvent(p->stop_done_event);
    return 0;
#else
    if (p->bit_buffer) {
        p->bit_buffer_filled_sem.release();
    }
    p->stop_done_sem.release();
#endif
}

int psijent_start(psijent* p)
{
    if (p->running) {
        return PSIJENT_RESULT_OK;
    }

#ifdef _MSC_VER
    ResetEvent(p->start_done_event);
#endif

    p->running = true;

#ifdef _MSC_VER
    HANDLE thread = CreateThread(nullptr, 0, background_thread_func, p, 0, nullptr);
    if (!thread) {
        p->running = false;
        return PSIJENT_RESULT_ERROR_UNEXPECTED;
    }
    CloseHandle(thread);
#else
    std::thread thread([p] { background_thread_func(p); });
    if (!thread.joinable()) {
        p->running = false;
        return PSIJENT_RESULT_ERROR_UNEXPECTED;
    }
    thread.detach();
#endif

#ifdef _MSC_VER
    WaitForSingleObject(p->start_done_event, INFINITE);
#else
    p->start_done_sem.acquire();
#endif

    return PSIJENT_RESULT_OK;
}

int psijent_stop(psijent* p)
{
    if (!p) {
        return PSIJENT_RESULT_ERROR_UNEXPECTED;
    }

#ifdef _MSC_VER
    ResetEvent(p->stop_done_event);
#endif

    p->running = false;

#ifdef _MSC_VER
    WaitForSingleObject(p->stop_done_event, INFINITE);
#else
    p->stop_done_sem.acquire();
#endif

    return PSIJENT_RESULT_OK;
}

int psijent_randbits(psijent* p, uint8_t* dest, const int length)
{
    std::lock_guard lock(p->randbits_mtx);

    if (!p->running) {
        return PSIJENT_RESULT_ERROR_NOT_RUNNING;
    }

#ifdef _MSC_VER
    ResetEvent(p->bit_buffer_filled_event);
#endif

    p->bit_buffer_idx = 0;
    p->bit_buffer_length = length;
    p->bit_buffer = dest;

#ifdef _MSC_VER
    WaitForSingleObject(p->bit_buffer_filled_event, INFINITE);
#else
    p->bit_buffer_filled_sem.acquire();
#endif

    if (p->bit_buffer_idx < length) {
        return PSIJENT_RESULT_INTERRUPTED;
    }

    return PSIJENT_RESULT_OK;
}

int psijent_randbytes(psijent* p, uint8_t* dest, const int length)
{
    const int buffer_length = length * 8;
    const auto bit_buffer = std::make_unique<uint8_t[]>(buffer_length);
    if (int result; (result = psijent_randbits(p, bit_buffer.get(), buffer_length)) != PSIJENT_RESULT_OK) {
        return result;
    }
    for (int i = 0; i < length; ++i) {
        const int base = i * 8;
        dest[i] =
            (bit_buffer[base] << 0) |
            (bit_buffer[base + 1] << 1) |
            (bit_buffer[base + 2] << 2) |
            (bit_buffer[base + 3] << 3) |
            (bit_buffer[base + 4] << 4) |
            (bit_buffer[base + 5] << 5) |
            (bit_buffer[base + 6] << 6) |
            (bit_buffer[base + 7] << 7);
    }
    return PSIJENT_RESULT_OK;
}

int psijent_randuniform(psijent* p, double* dest, const int length, const int mantissa_length)
{
    const int buffer_length = length * mantissa_length;
    const auto bit_buffer = std::make_unique<uint8_t[]>(buffer_length);

    if (const int result = psijent_randbits(p, bit_buffer.get(), buffer_length);
        result != PSIJENT_RESULT_OK) {
        return result;
    }

    int bit_idx = 0;

    for (int i = 0; i < length; ++i) {
        uint64_t mantissa_value = 0;
        for (int j = 0; j < mantissa_length; ++j) {
            mantissa_value |= static_cast<uint64_t>(bit_buffer[bit_idx++]) << (52 - mantissa_length + j);
        }
        dest[i] = std::bit_cast<double>(0x3FF0000000000000ull | mantissa_value) - 1.0;
    }

    return PSIJENT_RESULT_OK;
}

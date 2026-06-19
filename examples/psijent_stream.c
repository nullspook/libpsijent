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

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_MSC_VER)
#include <time.h>
#else
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

volatile sig_atomic_t stop = 0;

void sigint_handler(int signum)
{
    (void)signum;
    stop = 1;
}

static double now(void)
{
#if !defined(_MSC_VER)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#else
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#endif
}

int main(int argc, char* argv[])
{
    int chunk_size = 4096;
    int max_bytes = 0;

    int i = 1;
    for (; i < argc; ++i) {
        if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 < argc) {
                chunk_size = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Usage: %s [--chunk-size CHUNK_SIZE] [--max-bytes MAX_BYTES] OUTPUT\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--max-bytes") == 0) {
            if (i + 1 < argc) {
                max_bytes = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Usage: %s [--chunk-size CHUNK_SIZE] [--max-bytes MAX_BYTES] OUTPUT\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else {
            break;
        }
    }

    if (i >= argc) {
        fprintf(stderr, "Usage: %s [--chunk-size CHUNK_SIZE] [--max-bytes MAX_BYTES] OUTPUT\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE* output_file;

    char* output = argv[i];
    if (strcmp(output, "-") == 0) {
        output_file = stdout;
#ifdef _MSC_VER
        _setmode(_fileno(stdout), _O_BINARY);
#endif
    } else {
        output_file = fopen(output, "wb");
        if (output_file == NULL) {
            fprintf(stderr, "fopen failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    psijent* p = NULL;
    int result;

    if ((result = psijent_init(&p)) != 0) {
        fprintf(stderr, "psijent_init failed: %d\n", result);
        if (output_file != stdout) {
            fclose(output_file);
        }
        return EXIT_FAILURE;
    }

    uint8_t* buffer = malloc((size_t)chunk_size);
    if (buffer == NULL) {
        fprintf(stderr, "malloc failed\n");
        if (output_file != stdout) {
            fclose(output_file);
        }
        psijent_free(p);
        return EXIT_FAILURE;
    }

    signal(SIGINT, sigint_handler);

    int total_bytes = 0;
    double stream_start_time = now();

    while (!stop && (max_bytes <= 0 || total_bytes < max_bytes)) {
        double chunk_start_time = now();

        int bytes_to_generate = chunk_size;
        if (max_bytes > 0 && total_bytes + chunk_size > max_bytes) {
            bytes_to_generate = max_bytes - total_bytes;
        }

        psijent_randbytes(
            p,
            buffer,
            bytes_to_generate,
            false,  // decorrelate_with_lfsr
            true,   // mask_with_prng
            0       // bit_bias_amplification_level
        );

        int written = (int)fwrite(buffer, 1, (size_t)bytes_to_generate, output_file);
        if (written != bytes_to_generate) {
            fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
            if (output_file != stdout) {
                fclose(output_file);
            }
            free(buffer);
            psijent_free(p);
            return EXIT_FAILURE;
        }

        total_bytes += written;
        double total_kb = (double)total_bytes / 1000.0;
        double total_kib = (double)total_bytes / 1024.0;
        double total_kbit = total_kb * 8.0;

        double chunk_end_time = now();
        unsigned long total_seconds = (unsigned long)(chunk_end_time - stream_start_time);
        double kbit_per_sec = ((double)written * 8.0 / 1000.0) / (chunk_end_time - chunk_start_time);

        fprintf(stderr, "\r%d bytes (%.2f kB, %.2f KiB, %.2f kbit) generated, %lu s, %.2f kbit/s",
                total_bytes, total_kb, total_kib, total_kbit, total_seconds, kbit_per_sec);
    }

    fprintf(stderr, "\n");

    if (output_file != stdout) {
        fclose(output_file);
    }

    free(buffer);
    psijent_free(p);

    return EXIT_SUCCESS;
}
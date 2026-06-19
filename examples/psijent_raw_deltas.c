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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_MSC_VER)
#include <time.h>
#else
#include <windows.h>
#endif

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
    int count = 0;
    int warmup_sec = 0;

    int argi = 1;
    for (; argi < argc; ++argi) {
        if (strcmp(argv[argi], "--warmup-sec") == 0) {
            if (argi + 1 < argc) {
                warmup_sec = atoi(argv[++argi]);
            } else {
                fprintf(stderr, "Usage: %s [--warmup-sec SECONDS] COUNT\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else {
            break;
        }
    }

    if (argi >= argc) {
        fprintf(stderr, "Usage: %s [--warmup-sec SECONDS] COUNT\n", argv[0]);
        return EXIT_FAILURE;
    }

    count = atoi(argv[argi]);

    psijent* p = NULL;
    int result;

    if ((result = psijent_init(&p)) != 0) {
        fprintf(stderr, "psijent_init failed: %d\n", result);
        return EXIT_FAILURE;
    }

    uint64_t* values = malloc((size_t)count * sizeof(uint64_t));
    if (values == NULL) {
        fprintf(stderr, "malloc failed\n");
        psijent_free(p);
        return EXIT_FAILURE;
    }

    if (warmup_sec > 0) {
        double start = now();
        while (now() - start < warmup_sec) {
            psijent_raw_deltas(p, values, 1024);
        }
    }

    psijent_raw_deltas(p, values, count);

    for (int i = 0; i < count; ++i) {
        printf("%llu\n", (unsigned long long)values[i]);
    }

    free(values);
    psijent_free(p);
    return EXIT_SUCCESS;
}

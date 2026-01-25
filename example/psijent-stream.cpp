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

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef _MSC_VER
#include <fcntl.h>
#include <io.h>
#endif

volatile sig_atomic_t stop = 0;

extern "C" void sigint_handler(int)
{
    stop = 1;
}

int main(int argc, char* argv[])
{
    using namespace std::chrono;

    int chunk_size = 4096;
    int max_bytes = 0;

    int i = 1;
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--chunk-size") {
            if (i + 1 < argc) {
                chunk_size = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Usage: %s [--chunk-size CHUNK_SIZE] [--max-bytes MAX_BYTES] OUTPUT\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else if (arg == "--max-bytes") {
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

    std::string output = argv[i];
    if (output == "-") {
        output_file = stdout;
#ifdef _MSC_VER
        _setmode(_fileno(stdout), _O_BINARY);
#endif
    } else {
        output_file = fopen(output.c_str(), "wb");
        if (output_file == nullptr) {
            fprintf(stderr, "fopen failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    psijent* c = nullptr;
    int result;

    if ((result = psijent_init(&c)) != PSIJENT_RESULT_OK) {
        fprintf(stderr, "psijent_init failed: %d\n", result);
        if (output_file != stdout) {
            fclose(output_file);
        }
        return EXIT_FAILURE;
    }

    if ((result = psijent_start(c)) != PSIJENT_RESULT_OK) {
        fprintf(stderr, "psijent_start failed: %d\n", result);
        if (output_file != stdout) {
            fclose(output_file);
        }
        psijent_free(c);
        return EXIT_FAILURE;
    }

    std::vector<uint8_t> buffer(chunk_size);
    std::signal(SIGINT, sigint_handler);

    int total_bytes = 0;
    auto stream_start_time = steady_clock::now();

    while (!stop && (max_bytes <= 0 || total_bytes < max_bytes)) {
        auto chunk_start_time = steady_clock::now();

        int bytes_to_generate = chunk_size;
        if (max_bytes > 0 && total_bytes + chunk_size > max_bytes) {
            bytes_to_generate = max_bytes - total_bytes;
        }

        if ((result = psijent_randbytes(c, buffer.data(), bytes_to_generate)) != PSIJENT_RESULT_OK) {
            fprintf(stderr, "psijent_randbytes failed: %d\n", result);
            if (output_file != stdout) {
                fclose(output_file);
            }
            psijent_stop(c);
            psijent_free(c);
            return EXIT_FAILURE;
        }

        int written = fwrite(buffer.data(), 1, bytes_to_generate, output_file);
        if (written != bytes_to_generate) {
            fprintf(stderr, "fwrite failed: %s\n", strerror(errno));
            if (output_file != stdout) {
                fclose(output_file);
            }
            psijent_stop(c);
            psijent_free(c);
            return EXIT_FAILURE;
        }

        total_bytes += written;
        double total_kb = static_cast<double>(total_bytes) / 1000.0;
        double total_kib = static_cast<double>(total_bytes) / 1024.0;
        double total_kbit = total_kb * 8.0;

        auto chunk_end_time = steady_clock::now();
        unsigned long total_seconds = duration_cast<seconds>(chunk_end_time - stream_start_time).count();
        double kbit_per_sec = (static_cast<double>(written) * 8.0 / 1000.0) /
            duration<double>(chunk_end_time - chunk_start_time).count();

        fprintf(stderr, "\r%d bytes (%.2f kB, %.2f KiB, %.2f kbit) generated, %lu s, %.2f kbit/s",
            total_bytes, total_kb, total_kib, total_kbit, total_seconds, kbit_per_sec);
    }

    fprintf(stderr, "\n");

    if (output_file != stdout) {
        fclose(output_file);
    }

    psijent_stop(c);
    psijent_free(c);

    return EXIT_SUCCESS;
}
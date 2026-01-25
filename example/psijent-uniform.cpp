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

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char* argv[])
{
    psijent* p = nullptr;
    int result;

    int count = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s COUNT\n", argv[0]);
        return EXIT_FAILURE;
    }

    count = std::atoi(argv[1]);

    if ((result = psijent_init(&p)) != PSIJENT_RESULT_OK) {
        fprintf(stderr, "psijent_init failed: %d\n", result);
        return EXIT_FAILURE;
    }

    if ((result = psijent_start(p)) != PSIJENT_RESULT_OK) {
        fprintf(stderr, "psijent_start failed: %d\n", result);
        psijent_free(p);
        return EXIT_FAILURE;
    }

    std::vector<double> values(count);
    if ((result = psijent_randuniform(p, values.data(), count, 23)) != PSIJENT_RESULT_OK) {
        fprintf(stderr, "psijent_randuniform failed: %d\n", result);
        psijent_stop(p);
        psijent_free(p);
        return EXIT_FAILURE;
    }

    for (double value : values) {
        printf("%.9f\n", value);
    }

    psijent_stop(p);
    psijent_free(p);

    return EXIT_SUCCESS;
}
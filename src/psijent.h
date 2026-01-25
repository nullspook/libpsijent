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

#ifdef __cplusplus
extern "C" {
#endif

#define PSIJENT_RESULT_OK 0
#define PSIJENT_RESULT_HW_TIMEKEEPER_UNSUPPORTED (-61)
#define PSIJENT_RESULT_ERROR_NOT_RUNNING (-36)
#define PSIJENT_RESULT_INTERRUPTED (-92)
#define PSIJENT_RESULT_ERROR_UNEXPECTED (-1)

typedef struct psijent psijent;

int psijent_init(psijent** p);
void psijent_free(psijent* p);
int psijent_start(psijent* p);
int psijent_stop(psijent* p);
int psijent_randbits(psijent* p, uint8_t* dest, int length);
int psijent_randbytes(psijent* p, uint8_t* dest, int length);
int psijent_randuniform(psijent* p, double* dest, int length, int mantissa_length);

#ifdef __cplusplus
}
#endif

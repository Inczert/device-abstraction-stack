// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_RESULT_H
#define DAS_RESULT_H

typedef enum das_result {
    DAS_OK = 0,
    DAS_ERROR_INVALID_ARGUMENT = -1,
    DAS_ERROR_UNSUPPORTED = -2
} das_result_t;

#endif

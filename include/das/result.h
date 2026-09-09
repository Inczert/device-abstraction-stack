// SPDX-License-Identifier: Apache-2.0

#ifndef DAS_RESULT_H
#define DAS_RESULT_H

/** Common DAS operation result. */
typedef enum das_result {
    /** Operation completed successfully. */
    DAS_OK = 0,
    /** One or more arguments are invalid for the selected backend/device. */
    DAS_ERROR_INVALID_ARGUMENT = -1,
    /** The operation is valid generically but unsupported by this backend/device. */
    DAS_ERROR_UNSUPPORTED = -2,
    /** A bounded hardware/state transition did not complete before its deadline. */
    DAS_ERROR_TIMEOUT = -3
} das_result_t;

#endif

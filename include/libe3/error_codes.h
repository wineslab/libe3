/**
 * @file error_codes.h
 * @brief libe3 error codes - shared C/C++ definitions
 *
 * Provides a single source of truth for all libe3 error codes via an
 * X-macro list. Both the C API (@ref e3_error_code_e) and the C++ API
 * (@ref libe3::ErrorCode) are generated from this list.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// Shared X-macro list of libe3 error codes and C helpers
#ifndef LIBE3_ERROR_CODES_H
#define LIBE3_ERROR_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup error_codes libe3 Error Codes
 * @brief Error codes returned by libe3 operations.
 *
 * All libe3 API functions that can fail return either @ref libe3::ErrorCode
 * (C++ API) or @ref e3_error_t (C API). A return value of 0 always means
 * success; all error values are negative.
 * @{
 */

/**
 * @brief X-macro list of all libe3 error codes.
 *
 * Expand with `#define X(name, val) ...` before using this macro,
 * and `#undef X` afterwards. Each entry exposes a symbolic name and its
 * corresponding integer value.
 *
 * | Name                     | Value | Meaning                                        |
 * |--------------------------|-------|------------------------------------------------|
 * | SUCCESS                  |     0 | Operation completed successfully               |
 * | INVALID_PARAM            |    -1 | One or more parameters are invalid             |
 * | NOT_INITIALIZED          |    -2 | Component has not been initialized yet         |
 * | ALREADY_INITIALIZED      |    -3 | Component is already initialized               |
 * | NOT_CONNECTED            |    -4 | No active connection available                 |
 * | CONNECTION_FAILED        |    -5 | Failed to establish a connection               |
 * | TIMEOUT                  |    -6 | Operation timed out                            |
 * | ENCODE_FAILED            |    -7 | PDU encoding failed                            |
 * | DECODE_FAILED            |    -8 | PDU decoding failed                            |
 * | SM_NOT_FOUND             |    -9 | Requested Service Model was not found          |
 * | SM_ALREADY_REGISTERED    |   -10 | Service Model is already registered            |
 * | BUFFER_TOO_SMALL         |   -11 | Provided buffer is too small                   |
 * | INTERNAL_ERROR           |   -12 | Unexpected internal error                      |
 * | SUBSCRIPTION_EXISTS      |   -13 | Subscription already exists                    |
 * | SUBSCRIPTION_NOT_FOUND   |   -14 | Subscription was not found                     |
 * | DAPP_NOT_REGISTERED      |   -15 | Requested dApp is not registered               |
 * | TRANSPORT_ERROR          |   -16 | Transport layer error                          |
 * | STATE_ERROR              |   -17 | Operation not valid in current state           |
 * | SM_START_FAILED          |   -18 | Service Model failed to start                  |
 * | NOT_FOUND                |   -19 | Requested resource was not found               |
 * | SM_ERROR_INVALID_PARAM   |   -20 | Service Model received invalid parameter       |
 * | SM_ERROR_NOT_FOUND       |   -21 | Service Model resource not found               |
 * | SM_ERROR_ALREADY_EXISTS  |   -22 | Service Model resource already exists          |
 * | SM_ERROR_THREAD_FAILED   |   -23 | Service Model thread failed to start           |
 * | SM_ERROR_MEMORY          |   -24 | Service Model memory allocation failure        |
 * | CANCELLED                |   -25 | Operation was cancelled                        |
 * | GENERIC_ERROR            |  -100 | Unclassified generic error                     |
 */
#define LIBE3_ERROR_CODE_LIST \
    X(SUCCESS, 0) \
    X(INVALID_PARAM, -1) \
    X(NOT_INITIALIZED, -2) \
    X(ALREADY_INITIALIZED, -3) \
    X(NOT_CONNECTED, -4) \
    X(CONNECTION_FAILED, -5) \
    X(TIMEOUT, -6) \
    X(ENCODE_FAILED, -7) \
    X(DECODE_FAILED, -8) \
    X(SM_NOT_FOUND, -9) \
    X(SM_ALREADY_REGISTERED, -10) \
    X(BUFFER_TOO_SMALL, -11) \
    X(INTERNAL_ERROR, -12) \
    X(SUBSCRIPTION_EXISTS, -13) \
    X(SUBSCRIPTION_NOT_FOUND, -14) \
    X(DAPP_NOT_REGISTERED, -15) \
    X(TRANSPORT_ERROR, -16) \
    X(STATE_ERROR, -17) \
    X(SM_START_FAILED, -18) \
    X(NOT_FOUND, -19) \
    X(SM_ERROR_INVALID_PARAM, -20) \
    X(SM_ERROR_NOT_FOUND, -21) \
    X(SM_ERROR_ALREADY_EXISTS, -22) \
    X(SM_ERROR_THREAD_FAILED, -23) \
    X(SM_ERROR_MEMORY, -24) \
    X(CANCELLED, -25) \
    X(GENERIC_ERROR, -100)

/** @} */ /* end of error_codes group */

/**
 * @brief C-compatible error code enumeration.
 *
 * Values match those in @ref libe3::ErrorCode. Use the `E3_` prefix
 * to avoid collisions with C++ symbols.
 */
enum e3_error_code_e {
#define X(name, val) E3_##name = val,
    LIBE3_ERROR_CODE_LIST
#undef X
};

/**
 * @brief C error type alias.
 *
 * An `int` whose value is one of the @ref e3_error_code_e constants.
 * Zero means success; all error values are negative.
 */
typedef int e3_error_t;

/**
 * @brief Convert a numeric error code to a human-readable name.
 *
 * @param code Error code value (one of @ref e3_error_code_e)
 * @return Null-terminated string with the symbolic name, or
 *         `"UNKNOWN_ERROR"` if the value is not recognised.
 */
const char* e3_error_to_string(e3_error_t code);

#ifdef __cplusplus
}
#endif

#endif // LIBE3_ERROR_CODES_H

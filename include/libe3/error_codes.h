// Shared X-macro list of libe3 error codes and C helpers
#ifndef LIBE3_ERROR_CODES_H
#define LIBE3_ERROR_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Single source of truth for error codes. Use the macro list with a
 * temporary definition of X to generate enums, switch cases, etc.
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

/* C-friendly prefixed enum values to avoid global-name collisions */
enum e3_error_code_e {
#define X(name, val) E3_##name = val,
    LIBE3_ERROR_CODE_LIST
#undef X
};

typedef int e3_error_t;

/* Convert a numeric error code to a human-readable name (C API) */
const char* e3_error_to_string(e3_error_t code);

#ifdef __cplusplus
}
#endif

#endif // LIBE3_ERROR_CODES_H

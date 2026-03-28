/*
 * C API for libe3
 * Minimal C-compatible wrappers to create an E3Agent and ServiceModel from C
 */
#ifndef LIBE3_C_API_H
#define LIBE3_C_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "libe3/error_codes.h"

/* Opaque handles */
typedef struct e3_agent_handle_s e3_agent_handle_t;
typedef struct e3_service_model_handle_s e3_service_model_handle_t;


/* Callback prototypes for a C-backed ServiceModel */
typedef e3_error_t (*e3_sm_init_cb)(void* sm_context);
typedef void      (*e3_sm_destroy_cb)(void* sm_context);
typedef e3_error_t (*e3_sm_start_cb)(void* sm_context);
typedef void      (*e3_sm_stop_cb)(void* sm_context);
typedef int       (*e3_sm_is_running_cb)(void* sm_context); /* returns 0/1 */
typedef e3_error_t (*e3_sm_process_control_cb)(
    e3_service_model_handle_t* sm_handle,
    void* sm_context,
    uint32_t request_message_id,
    uint32_t dapp_id,
    uint32_t ran_function_id,
    uint32_t control_id,
    const uint8_t* data,
    size_t data_len
);

/* Callback for incoming dApp reports (dApp -> RAN). */
typedef void (*e3_dapp_report_cb)(
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const uint8_t* report_data,
    size_t report_data_len
);

/** Callback when dApp status changes (connect, disconnect, subscribe, unsubscribe). */
typedef void (*e3_dapp_status_changed_cb)(void);

/**
 * @brief Set callback for dApp status changes.
 *
 * Called when a dApp connects, disconnects, subscribes, or unsubscribes.
 * Pass NULL as \p handler to clear the callback.
 *
 * @param agent Agent handle
 * @param handler Callback invoked on each dApp status change
 * @return \ref e3_error_t (see \ref libe3::ErrorCode; 0 == SUCCESS)
 */
e3_error_t e3_agent_set_dapp_status_changed_handler(
    e3_agent_handle_t* agent,
    e3_dapp_status_changed_cb handler
);

/**
 * @brief Descriptor describing a ServiceModel implemented in C.
 *
 * - `name`, `version` and `ran_function_id` identify the SM.
 * - `telemetry_ids` and `control_ids` point to arrays that will be copied
 *   by the implementation; the caller may free them after creating the SM.
 * - Callbacks are invoked by the C++ adapter and receive `user_data`.
 */
typedef struct {
    const char* name;                // Service Model name (null-terminated string)
    uint32_t version;                // Service Model version
    uint32_t ran_function_id;        // RAN function ID for this SM

    const uint32_t* telemetry_ids;   // Array of supported telemetry IDs (copied by library)
    size_t telemetry_ids_len;        // Number of telemetry IDs

    const uint32_t* control_ids;     // Array of supported control IDs (copied by library)
    size_t control_ids_len;          // Number of control IDs

    /* Optional opaque RAN function data (E3SM-encoded, sent in setup response) */
    const uint8_t* ran_function_data; // Pointer to opaque RAN function data (may be NULL)
    size_t ran_function_data_len;     // Length of ran_function_data in bytes (0 if none)

    /* Callbacks */
    e3_sm_init_cb sm_init;                   // Called on SM registration
    e3_sm_destroy_cb sm_destroy;             // Called on SM destruction
    e3_sm_start_cb sm_start;                 // Called when SM should start
    e3_sm_stop_cb sm_stop;                   // Called when SM should stop
    e3_sm_is_running_cb sm_is_running;       // Called to query running state
    e3_sm_process_control_cb sm_process_control; // Called to process control actions

    void* sm_context;                 // Opaque pointer passed to all callbacks
} e3_c_service_model_desc_t;

/* Create/destroy a C-backed ServiceModel */
/**
 * @brief Create a ServiceModel implementation backed by C callbacks.
 *
 * The returned pointer owns a C++ `ServiceModel` instance. The caller
 * retains ownership until the model is registered with an agent via
 * `e3_agent_register_sm`, at which point ownership is transferred to the
 * agent and `e3_service_model_destroy` will no longer free the object.
 *
 * @param desc Descriptor containing metadata and callback function pointers
 * @return e3_service_model_handle_t* on success, NULL on allocation error
 */
e3_service_model_handle_t* e3_service_model_create_from_c(
    const e3_c_service_model_desc_t* desc
);

/**
 * @brief Destroy a C-backed ServiceModel handle.
 *
 * If the model has been registered with an agent (ownership transferred),
 * this function is a no-op and the agent takes responsibility for cleanup.
 */
void e3_service_model_destroy(e3_service_model_handle_t* sm);

/**
 * @brief Emit an indication message from a ServiceModel to the outbound queue.
 *
 * This is intended for C-backed ServiceModels. The message is forwarded by
 * the internal publisher path.
 */
e3_error_t e3_service_model_emit_indication(
    e3_service_model_handle_t* sm,
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const uint8_t* data,
    size_t data_len
);

/**
 * @brief Emit a message acknowledgment from a ServiceModel.
 */
e3_error_t e3_service_model_emit_message_ack(
    e3_service_model_handle_t* sm,
    uint32_t request_id,
    int response_code
);

/**
 * @brief Configuration for creating an E3Agent from C.
 *
 * All string pointers are copied by the library; the caller may free them
 * after the call. Use NULL for optional strings to keep library defaults.
 *
 * Enum values: link_layer: 0=ZMQ, 1=POSIX. transport_layer: 0=SCTP, 1=TCP, 2=IPC.
 * encoding: 0=ASN1, 1=JSON. Use -1 for link_layer, transport_layer, encoding, log_level
 * to keep defaults. Use 0 for setup_port, subscriber_port, publisher_port to keep defaults.
 */
typedef struct {
    const char* ran_identifier;
    int link_layer;       /* 0=ZMQ, 1=POSIX, -1=default */
    int transport_layer;  /* 0=SCTP, 1=TCP, 2=IPC, -1=default */
    uint16_t setup_port;
    uint16_t subscriber_port;
    uint16_t publisher_port;
    const char* setup_endpoint;
    const char* subscriber_endpoint;
    const char* publisher_endpoint;
    int encoding;         /* 0=ASN1, 1=JSON, -1=default */
    size_t io_threads;    /* 0=default */
    int log_level;        /* -1=default */
} e3_config_t;

/**
 * @brief Create/destroy an `E3Agent` handle
 */
/**
 * @brief Create a new `E3Agent` with a default configuration.
 *
 * The returned handle must be destroyed with `e3_agent_destroy` when no
 * longer needed.
 *
 * @return e3_agent_handle_t* on success, NULL on allocation error
 */
e3_agent_handle_t* e3_agent_create_default();

/**
 * @brief Create a new `E3Agent` with the given configuration.
 *
 * If \p config is NULL, behaves like \ref e3_agent_create_default.
 * String fields in \p config are copied; the caller may free them after the call.
 * Use 0 or NULL for fields to keep library defaults (e.g. setup_port 0 → 9990).
 *
 * @param config Configuration (ports, endpoints, link/transport layer, etc.)
 * @return e3_agent_handle_t* on success, NULL on allocation error
 */
e3_agent_handle_t* e3_agent_create_with_config(const e3_config_t* config);

/**
 * @brief Destroy an `E3Agent` handle.
 *
 * This will free the underlying resources. If the agent is currently
 * running, callers should stop it with `e3_agent_stop` first.
 */
void e3_agent_destroy(e3_agent_handle_t* agent);

/* Agent operations */
/**
 * @brief Initialize the agent.
 *
 * Prepares internal resources; must be called before \ref e3_agent_start.
 * Returns a \ref e3_error_t (see \ref libe3::ErrorCode; 0 == SUCCESS).
 */
e3_error_t e3_agent_init(e3_agent_handle_t* agent);

/**
 * @brief Start the agent processing threads and accept dApp connections.
 *
 * Returns a \ref e3_error_t (see \ref libe3::ErrorCode; 0 == SUCCESS).
 */
e3_error_t e3_agent_start(e3_agent_handle_t* agent);

/**
 * @brief Stop the agent and release runtime resources.
 *
 * Safe to call even if the agent is not running.
 * Returns nothing; errors are ignored.
 */
void e3_agent_stop(e3_agent_handle_t* agent);

/* Query agent state */
/**
 * @brief Get the agent state as an integer matching `libe3::AgentState`.
 *
 * Useful for callers in C to inspect agent status. Returns an `int` with
 * the enum value; `AgentState::ERROR` is returned on invalid handle.
 */
int e3_agent_get_state(e3_agent_handle_t* agent);

/**
 * @brief Query whether the agent is currently running.
 *
 * @return 1 if running, 0 otherwise (or on invalid handle)
 */
int e3_agent_is_running(e3_agent_handle_t* agent);

/**
 * @brief Set callback for incoming dApp reports (dApp -> RAN).
 *
 * Pass NULL as \p handler to clear the callback.
 *
 * @param agent Agent handle
 * @param handler Callback invoked for each incoming dApp report
 * @return \ref e3_error_t (see \ref libe3::ErrorCode; 0 == SUCCESS)
 */
e3_error_t e3_agent_set_dapp_report_handler(
    e3_agent_handle_t* agent,
    e3_dapp_report_cb handler
);

/* Helpers to return arrays allocated by the library. Caller must free with
 * `e3_agent_free_uint32_array` when finished.
 */
/**
 * @brief Return a newly-allocated array of available RAN function IDs.
 *
 * The returned array is allocated with `malloc` and the caller must free
 * it with `e3_agent_free_uint32_array`. The number of elements is written
 * to `out_len` if non-NULL. Returns NULL and sets `out_len` to 0 if there
 * are no entries or the handle is invalid.
 */
uint32_t* e3_agent_get_available_ran_functions(e3_agent_handle_t* agent, size_t* out_len);

/**
 * @brief Return a newly-allocated array of registered dApp IDs.
 *
 * See `e3_agent_get_available_ran_functions` for ownership semantics.
 */
uint32_t* e3_agent_get_registered_dapps(e3_agent_handle_t* agent, size_t* out_len);

/**
 * @brief Return a newly-allocated array of RAN function IDs subscribed by a dApp.
 *
 * @param dapp_id dApp identifier to query
 */
uint32_t* e3_agent_get_dapp_subscriptions(e3_agent_handle_t* agent, uint32_t dapp_id, size_t* out_len);

/**
 * @brief Return a newly-allocated array of dApp IDs subscribed to a RAN function.
 *
 * @param ran_function_id RAN function identifier to query
 */
uint32_t* e3_agent_get_ran_function_subscribers(e3_agent_handle_t* agent, uint32_t ran_function_id, size_t* out_len);

/**
 * @brief Free an array previously returned by the e3_agent_get_* helpers.
 */
void e3_agent_free_uint32_array(uint32_t* arr);

/**
 * @brief Send an indication message to a specific dApp.
 *
 * @param agent Agent handle
 * @param dapp_id Target dApp identifier
 * @param ran_function_id RAN function identifier
 * @param data Pointer to E3SM-encoded payload (may be NULL)
 * @param data_len Length of payload in bytes
 * @return \ref e3_error_t (see \ref libe3::ErrorCode; 0 == SUCCESS)
 */
e3_error_t e3_agent_send_indication(
    e3_agent_handle_t* agent,
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const uint8_t* data,
    size_t data_len
);

/**
 * @brief Send an xApp control action to a specific dApp (e.g. from E2/SM).
 *
 * @param agent Agent handle
 * @param dapp_id Target dApp identifier
 * @param ran_function_id RAN function identifier
 * @param control_data Pointer to E3SM-encoded control payload (may be NULL if data_len is 0)
 * @param control_data_len Length of payload in bytes
 * @return \ref e3_error_t (see \ref libe3::ErrorCode; 0 == SUCCESS)
 */
e3_error_t e3_agent_send_xapp_control(
    e3_agent_handle_t* agent,
    uint32_t dapp_id,
    uint32_t ran_function_id,
    const uint8_t* control_data,
    size_t control_data_len
);

/**
 * @brief Send a message acknowledgment (e.g. ack a control/request from dApp).
 *
 * @param agent Agent handle
 * @param request_id ID of the request being acknowledged
 * @param response_code 0 = positive (success), 1 = negative (failure)
 * @return \ref e3_error_t (see \ref libe3::ErrorCode; 0 == SUCCESS)
 */
e3_error_t e3_agent_send_message_ack(
    e3_agent_handle_t* agent,
    uint32_t request_id,
    int response_code
);

/* Simple stats */
/**
 * @brief Number of registered dApps known to the agent.
 */
size_t e3_agent_dapp_count(e3_agent_handle_t* agent);

/**
 * @brief Total number of active subscriptions tracked by the agent.
 */
size_t e3_agent_subscription_count(e3_agent_handle_t* agent);

/**
 * @brief Register a `ServiceModel` implementation with the agent.
 *
 * Ownership of `sm` is transferred to the agent on success and callers
 * must not free the handle after a successful call. On failure the caller
 * retains ownership and should call `e3_service_model_destroy`.
 *
 * @return e3_error_t ErrorCode value (0 == success)
 */
e3_error_t e3_agent_register_sm(
    e3_agent_handle_t* agent,
    e3_service_model_handle_t* sm
);

#ifdef __cplusplus
}
#endif

#endif // LIBE3_C_API_H

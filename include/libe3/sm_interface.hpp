/**
 * @file sm_interface.hpp
 * @brief Service Model Interface - Extension point for RAN vendors
 *
 * Defines the interface that Service Models must implement to integrate
 * with the E3 agent. E3SM logic is treated as opaque to E3AP - this
 * interface provides extension points without embedding SM-specific logic.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_SM_INTERFACE_HPP
#define LIBE3_SM_INTERFACE_HPP

#include "types.hpp"
#include <memory>
#include <optional>
#include <functional>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

namespace libe3 {

// Forward declarations
class ServiceModel;

/**
 * @brief Abstract Service Model interface
 *
 * This is the extension point for RAN vendors to implement their
 * Service Models. The E3 agent treats E3SM as opaque - all SM-specific
 * encoding/decoding happens in the implementations of this interface.
 *
 * RAN vendors should:
 * 1. Derive from this class for each supported SM
 * 2. Implement the required virtual methods
 * 3. Register their SMs with the E3Agent
 */
class ServiceModel {
public:
    virtual ~ServiceModel() = default;

    /**
     * @brief Get SM name
     * @return The name of the SM
     */
    virtual std::string name() const = 0;

    /**
     * @brief Get SM version
     * @return The version number of the SM
     */
    virtual uint32_t version() const = 0;

    /**
     * @brief Get RAN function ID for this SM
     * @return The RAN function this SM serves
     */
    virtual uint32_t ran_function_id() const = 0;

    /**
     * @brief Get telemetry IDs supported by this SM
     * @return The telemetry identifiers the SM supports
     */
    virtual std::vector<uint32_t> telemetry_ids() const = 0;

    /**
     * @brief Get control IDs supported by this SM
     * @return The control identifiers the SM supports
     */
    virtual std::vector<uint32_t> control_ids() const = 0;

    /**
     * @brief RAN-function-specific opaque data (required, 1..32768 bytes)
     *
     * Returns the byte vector advertised for this RAN function in the
     * SetupResponse (ranFunctionList->ranFunctionData). E3AP declares the field
     * mandatory and non-empty -- `OCTET STRING (SIZE (1..262144))`, limited to
     * 32768 bytes -- so every SM
     * must override this: even the most minimal SM carries at least its own
     * name. An SM with nothing to advertise is not advertisable at all, since
     * ranFunctionList itself is optional but its entries are not.
     *
     * Registration fails with ErrorCode::INVALID_PARAM if the returned vector is
     * empty or longer than MAX_PROTOCOL_DATA_SIZE. The base implementation
     * returns an empty vector so that failure is a loud diagnostic rather than a
     * compile error in every existing SM; it is not a usable default.
     *
     * @return The bytes advertised for this RAN function; registration fails if
     *         the vector is empty or oversized.
     */
    virtual std::vector<uint8_t> ran_function_data() const { return {}; }

    /**
     * @brief The least time between two indications that this SM keeps for a subscription
     *
     * Sent to the dApp as the granted periodicity of its SubscriptionResponse.
     * Override it only when the SM itself keeps its indications at least that
     * far apart. The base implementation reports nothing, so the response then
     * carries no periodicity ("not reported").
     *
     * @param requested_us Periodicity the dApp asked for, in microseconds. 0 when
     *        it asked for none or for one indication per event.
     * @return Microseconds between two indications. std::nullopt or 0 when the SM
     *         keeps no interval or does not say; the response then has none.
     */
    virtual std::optional<uint32_t> granted_periodicity(uint32_t requested_us) const {
        (void)requested_us;
        return std::nullopt;
    }

    /**
     * @brief Initialize the SM
     *
     * Called when the SM is first registered.
     * @return ErrorCode::SUCCESS on success
     */
    virtual ErrorCode init() = 0;

    /**
     * @brief Destroy the SM and release resources
     *
     * Called when the SM is being unregistered.
     */
    virtual void destroy() = 0;

    /**
     * @brief Start the SM processing
     *
     * Called when the first dApp subscribes to this SM's RAN function.
     * @return ErrorCode::SUCCESS on success
     */
    virtual ErrorCode start() = 0;

    /**
     * @brief Stop the SM processing
     *
     * Called when the last dApp unsubscribes from this SM's RAN function.
     */
    virtual void stop() = 0;

    /**
     * @brief Check if SM is currently running
     * @return True while the SM is processing
     */
    virtual bool is_running() const = 0;

    /**
     * @brief Process a control action from a dApp
     *
     * Called by the E3 interface when a DAppControlAction is received for this
     * service model. Implementations answer it with respond_to_control() and can emit
     * outbound PDUs (e.g. IndicationMessage) via emit_outbound().
     * @param request_message_id Message identifier of the DAppControlAction
     * @param action The control action
     * @return ErrorCode::SUCCESS if the SM took the action
     */
    virtual ErrorCode handle_control_action(
        uint32_t request_message_id,
        const DAppControlAction& action
    ) = 0;

protected:
    ServiceModel() = default;

    /**
     * @brief Build a MessageAck PDU for a control/request message.
     *
     * DEPRECATED TO BE REMOVED. A Service Model answers a control with
     * respond_to_control(). This helper only builds the PDU.
     * @param request_message_id Message identifier of the message acknowledged
     * @param response_code Result to report
     * @return The PDU
     */
    static Pdu make_message_ack_pdu(
        uint32_t request_message_id,
        ResponseCode response_code
    ) {
        Pdu pdu(PduType::MESSAGE_ACK);
        MessageAck ack;
        ack.request_id = request_message_id;
        ack.response_code = response_code;
        pdu.choice = ack;
        return pdu;
    }

    /**
     * @brief Answer a dApp control
     *
     * Call it once per control, from any thread, when the outcome is known: from
     * handle_control_action() or later, as an SM that applies the control
     * asynchronously does. It sends a ControlResponse only when the dApp asked for
     * one (`request_ack`), so an SM can call it for every control. The RAN function
     * and the control identifier are filled in from the control. An SM that never
     * answers sends nothing: the library does not invent an answer for a control
     * that may still be applied.
     * @param dapp_id dApp that sent the control (`DAppControlAction::dapp_identifier`)
     * @param request_message_id Message identifier of the control, as passed to handle_control_action()
     * @param result Whether the control was applied, why not, and what the SM reports about it
     * @return ErrorCode::SUCCESS if the answer was queued; ErrorCode::NOT_FOUND if none is owed (the
     *         dApp did not ask, or the control was answered already); ErrorCode::INVALID_PARAM if the
     *         outcome is over MAX_CONTROL_OUTCOME_SIZE; ErrorCode::NOT_INITIALIZED if no agent runs the SM
     */
    ErrorCode respond_to_control(
        uint32_t dapp_id,
        uint32_t request_message_id,
        const ControlResult& result
    ) {
        if (!control_responder_) {
            return ErrorCode::NOT_INITIALIZED;
        }
        return control_responder_(dapp_id, request_message_id, result);
    }

    /**
     * @brief Build an IndicationMessage PDU.
     *
     * This helper only builds the PDU. Implementations can emit it with
     * emit_outbound() when appropriate.
     * @param dapp_id dApp the indication is for
     * @param ran_function_id RAN function the indication comes from
     * @param protocol_data Opaque payload of the SM
     * @return The PDU
     */
    static Pdu make_indication_pdu(
        uint32_t dapp_id,
        uint32_t ran_function_id,
        std::vector<uint8_t> protocol_data
    ) {
        Pdu pdu(PduType::INDICATION_MESSAGE);
        IndicationMessage msg;
        msg.dapp_identifier = dapp_id;
        msg.ran_function_identifier = ran_function_id;
        msg.protocol_data = std::move(protocol_data);
        pdu.choice = std::move(msg);
        return pdu;
    }

    /**
     * @brief Emit an outbound PDU produced by the SM.
     * @param pdu The PDU to send
     * @return ErrorCode::SUCCESS if it was queued, ErrorCode::NOT_INITIALIZED if no emitter is set
     */
    ErrorCode emit_outbound(Pdu&& pdu) {
        if (!outbound_emitter_) {
            return ErrorCode::NOT_INITIALIZED;
        }
        return outbound_emitter_(std::move(pdu));
    }

    /**
     * @brief Get currently subscribed dApps for this SM's RAN function.
     * @return The identifiers of the subscribed dApps, empty when no provider is set
     */
    std::vector<uint32_t> get_subscribers() const {
        if (!subscribers_provider_) {
            return {};
        }
        return subscribers_provider_();
    }

    /**
     * @brief Set the outbound PDU emitter callback (used by E3Interface).
     * @param emitter Callback that queues a PDU
     */
    void set_outbound_emitter(std::function<ErrorCode(Pdu&&)> emitter) {
        outbound_emitter_ = std::move(emitter);
    }

    /**
     * @brief Set the control responder callback (used by E3Interface).
     * @param responder Callback that answers a dApp control
     */
    void set_control_responder(std::function<ErrorCode(uint32_t, uint32_t, const ControlResult&)> responder) {
        control_responder_ = std::move(responder);
    }

    /**
     * @brief Set subscriber provider callback (used by E3Interface).
     * @param provider Callback that lists the subscribed dApps
     */
    void set_subscribers_provider(std::function<std::vector<uint32_t>()> provider) {
        subscribers_provider_ = std::move(provider);
    }

private:
    std::function<ErrorCode(Pdu&&)> outbound_emitter_;
    std::function<ErrorCode(uint32_t, uint32_t, const ControlResult&)> control_responder_;
    std::function<std::vector<uint32_t>()> subscribers_provider_;

    friend class E3Interface;
};

/**
 * @brief Factory function type for creating SM instances
 */
using SmFactory = std::function<std::unique_ptr<ServiceModel>()>;

/**
 * @brief SM Registry for managing registered Service Models
 *
 * This class provides a central registry for Service Models.
 * It's used by the E3Agent to find and manage SMs.
 */
class SmRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return The registry
     */
    static SmRegistry& instance();

    /**
     * @brief Register a Service Model
     *
     * @param sm Service Model to register
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::SM_ALREADY_REGISTERED if SM for this RAN function exists
     * @return ErrorCode::INVALID_PARAM if the SM lists more than 256 telemetry or
     *         control ids, or 64 RAN functions are already registered
     */
    ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    /**
     * @brief Register a Service Model factory
     *
     * Use this to defer SM creation until needed.
     * @param ran_function_id RAN function the factory serves
     * @param factory Creates the SM
     * @return ErrorCode::SUCCESS on success
     */
    ErrorCode register_sm_factory(uint32_t ran_function_id, SmFactory factory);

    /**
     * @brief Unregister a Service Model by RAN function ID
     * @param ran_function_id RAN function of the SM to remove
     * @return ErrorCode::SUCCESS on success, an error code otherwise
     */
    ErrorCode unregister_sm(uint32_t ran_function_id);

    /**
     * @brief Get SM by RAN function ID
     *
     * @param ran_function_id RAN function ID to look up
     * @return Pointer to SM, nullptr if not found
     */
    ServiceModel* get_by_ran_function(uint32_t ran_function_id);

    /**
     * @brief Get all available RAN function IDs
     * @return The RAN functions that have an SM or a factory
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Start SM for a RAN function
     * @param ran_function_id RAN function whose SM is started
     * @return ErrorCode::SUCCESS on success, an error code otherwise
     */
    ErrorCode start_sm(uint32_t ran_function_id);

    /**
     * @brief Stop SM for a RAN function
     * @param ran_function_id RAN function whose SM is stopped
     * @return ErrorCode::SUCCESS on success, an error code otherwise
     */
    ErrorCode stop_sm(uint32_t ran_function_id);

    /**
     * @brief Check if SM is running
     * @param ran_function_id RAN function whose SM is asked about
     * @return True if its SM is running
     */
    bool is_sm_running(uint32_t ran_function_id) const;

    /**
     * @brief Clear all registered SMs
     */
    void clear();

private:
    SmRegistry() = default;
    ~SmRegistry() = default;
    SmRegistry(const SmRegistry&) = delete;
    SmRegistry& operator=(const SmRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, std::unique_ptr<ServiceModel>> sms_;
    std::unordered_map<uint32_t, SmFactory> factories_;
};

} // namespace libe3

#endif // LIBE3_SM_INTERFACE_HPP

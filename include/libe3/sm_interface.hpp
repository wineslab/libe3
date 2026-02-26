/**
 * @file sm_interface.hpp
 * @brief Service Model Interface - Extension point for RAN vendors
 *
 * Defines the interface that Service Models must implement to integrate
 * with the E3 agent. E3SM logic is treated as opaque to E3AP - this
 * interface provides extension points without embedding SM-specific logic.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_SM_INTERFACE_HPP
#define LIBE3_SM_INTERFACE_HPP

#include "types.hpp"
#include <memory>
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
     */
    virtual std::string name() const = 0;

    /**
     * @brief Get SM version
     */
    virtual uint32_t version() const = 0;

    /**
     * @brief Get RAN function ID for this SM
     */
    virtual uint32_t ran_function_id() const = 0;

    /**
     * @brief Get telemetry IDs supported by this SM
     */
    virtual std::vector<uint32_t> telemetry_ids() const = 0;

    /**
     * @brief Get control IDs supported by this SM
     */
    virtual std::vector<uint32_t> control_ids() const = 0;

    /**
     * @brief Optional RAN-function-specific opaque data
     *
     * Returns a byte vector that will be included in the SetupResponse
     * as additional payload (ranFunctionList->ranFunctionData OCTET STRING for ASN).
     * Default is empty since it is optional.
     */
    virtual std::vector<uint8_t> ran_function_data() const { return {}; }

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
     */
    virtual bool is_running() const = 0;

    /**
     * @brief Process a control action from a dApp
     *
     * Called by the E3 interface when a DAppControlAction is received for this
     * service model. Implementations can emit outbound PDUs (e.g. MessageAck,
     * IndicationMessage) via emit_outbound().
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
     * This helper only builds the PDU. Implementations can decide whether to
     * emit it with emit_outbound() or suppress it when protocol semantics allow.
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
     * @brief Build an IndicationMessage PDU.
     *
     * This helper only builds the PDU. Implementations can emit it with
     * emit_outbound() when appropriate.
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
     */
    ErrorCode emit_outbound(Pdu&& pdu) {
        if (!outbound_emitter_) {
            return ErrorCode::NOT_INITIALIZED;
        }
        return outbound_emitter_(std::move(pdu));
    }

    /**
     * @brief Get currently subscribed dApps for this SM's RAN function.
     */
    std::vector<uint32_t> get_subscribers() const {
        if (!subscribers_provider_) {
            return {};
        }
        return subscribers_provider_();
    }

    /**
     * @brief Set the outbound PDU emitter callback (used by E3Interface).
     */
    void set_outbound_emitter(std::function<ErrorCode(Pdu&&)> emitter) {
        outbound_emitter_ = std::move(emitter);
    }

    /**
     * @brief Set subscriber provider callback (used by E3Interface).
     */
    void set_subscribers_provider(std::function<std::vector<uint32_t>()> provider) {
        subscribers_provider_ = std::move(provider);
    }

private:
    std::function<ErrorCode(Pdu&&)> outbound_emitter_;
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
     */
    static SmRegistry& instance();

    /**
     * @brief Register a Service Model
     *
     * @param sm Service Model to register
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::SM_ALREADY_REGISTERED if SM for this RAN function exists
     */
    ErrorCode register_sm(std::unique_ptr<ServiceModel> sm);

    /**
     * @brief Register a Service Model factory
     *
     * Use this to defer SM creation until needed.
     */
    ErrorCode register_sm_factory(uint32_t ran_function_id, SmFactory factory);

    /**
     * @brief Unregister a Service Model by RAN function ID
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
     */
    std::vector<uint32_t> get_available_ran_functions() const;

    /**
     * @brief Start SM for a RAN function
     */
    ErrorCode start_sm(uint32_t ran_function_id);

    /**
     * @brief Stop SM for a RAN function
     */
    ErrorCode stop_sm(uint32_t ran_function_id);

    /**
     * @brief Check if SM is running
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

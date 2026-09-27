/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_CORE_CONTROLLER_SESSION_HPP
#define SHIO_CORE_CONTROLLER_SESSION_HPP

#include "shio/networking/interface/core_interface.hpp"
#include "shio/networking/stage_response/stage_response.hpp"
#include "shio/networking/stage_response/stage_response_ack.hpp"
#include "shio/networking/stage_response/stage_response_handshake.hpp"
#include "session.hpp"

#include <shio/utils/logging.hpp>
#include <shio/utils/options.hpp>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unistd.h>

namespace shio {

/**
 * CoreControllerSession class.
 * CoreControllerSession component serves as a liaison between the GlobalController (and its
 * control applications) and a lower (cluster) controller, through the CoreInterface.
 * Currently, the CoreControllerSession class contains the following variables:
 * - interface_: interface to submit requests.
 */
class CoreControllerSession : public Session {

private:
    CoreInterface interface_;

    /**
     * SendRule: Handle the rule to be submitted to the local controller.
     * @param user_address Local controller address.
     * @param rule Rule to be submitted.
     * @param operation ControlOperation.
     * @return PStatus::OK() if the rule was successfully executed,
     * PStatus::Error() otherwise.
     */
    PStatus SendRule (const std::string& user_address,
        const std::string& rule,
        ControlOperation* operation);

public:
    /**
     * CoreControllerSession parameterized constructor.
     * @param user_address User address identifier of the local controller.
     */
    explicit CoreControllerSession (const std::string& user_address);

    /**
     * CoreControllerSession parameterized constructor.
     * @param id Session identifier.
     * @param user_address User address identifier of the local controller.
     */
    CoreControllerSession (long id, const std::string& user_address);

    /**
     * CoreControllerSession parameterized constructor.
     * @param user_address User address identifier of the local controller (unused).
     * @param interface CoreInterface object to reuse.
     */
    CoreControllerSession (const std::string& user_address, CoreInterface interface);

    /**
     * CoreControllerSession default destructor.
     */
    ~CoreControllerSession ();

    /**
     * StartSession: Start session execution. Handles rules until the session is removed.
     * @param user_address User address identifier of the local controller.
     */
    void StartSession (const std::string& user_address);

    /**
     * ShareInterface: Share the interface.
     * @return Copy of the CoreInterface object (shares the same gRPC stub).
     */
    CoreInterface ShareInterface ();
};
} // namespace shio

#endif // SHIO_CORE_CONTROLLER_SESSION_HPP

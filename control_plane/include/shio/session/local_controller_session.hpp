/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_LOCAL_CONTROLLER_SESSION_HPP
#define CHEFERD_LOCAL_CONTROLLER_SESSION_HPP

#include "cheferd/networking/interface/local_interface.hpp"
#include "cheferd/networking/stage_response/stage_response.hpp"
#include "cheferd/networking/stage_response/stage_response_ack.hpp"
#include "cheferd/networking/stage_response/stage_response_handshake.hpp"
#include "session.hpp"

#include <cheferd/utils/logging.hpp>
#include <cheferd/utils/options.hpp>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <unistd.h>

namespace cheferd {

/**
 * LocalControllerSession class.
 * LocalControllerSession component serves as a liaison between the ClusterController (and its
 * control applications) and a local controller, through the LocalInterface.
 * Currently, the LocalControllerSession class contains the following variables:
 * - interface_: interface to submit requests.
 */
class LocalControllerSession : public Session {

private:
    LocalInterface interface_;

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
     * LocalControllerSession parameterized constructor.
     * @param user_address User address identifier of the local controller.
     */
    explicit LocalControllerSession (const std::string& user_address);

    /**
     * LocalControllerSession parameterized constructor.
     * @param id Session identifier.
     * @param user_address User address identifier of the local controller.
     */
    LocalControllerSession (long id, const std::string& user_address);

    /**
     * LocalControllerSession default destructor.
     */
    ~LocalControllerSession ();

    /**
     * StartSession: Start session execution. Handles rules until the session is removed.
     * @param user_address User address identifier of the local controller.
     */
    void StartSession (const std::string& user_address);
};
} // namespace cheferd

#endif // CHEFERD_LOCAL_CONTROLLER_SESSION_HPP

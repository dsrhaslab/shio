/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_CONNECTION_MANAGER_HPP
#define CHEFERD_CONNECTION_MANAGER_HPP

#include "cheferd/controller/control_application/control_application.hpp"
#include "cheferd/utils/options.hpp"
#include "cheferd/utils/status.hpp"

#include <thread>

namespace cheferd {

/**
 * ConnectionManager class.
 * The ConnectionManager class serves as base class for the connection manager component.
 */
class ConnectionManager {

public:
    /**
     * ConnectionManager default destructor.
     */
    virtual ~ConnectionManager () = default;

    /**
     * Start: Accept connections until Stop () is called. Blocks the calling thread.
     */
    virtual void Start () {};

    /**
     * Stop: Stop connection manager.
     */
    virtual void Stop () {};
};
} // namespace cheferd

#endif // CHEFERD_CONNECTION_MANAGER_HPP

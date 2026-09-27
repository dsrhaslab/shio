/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_CONNECTION_MANAGER_HPP
#define SHIO_CONNECTION_MANAGER_HPP

#include "shio/controller/control_application/control_application.hpp"
#include "shio/utils/options.hpp"
#include "shio/utils/status.hpp"

#include <thread>

namespace shio {

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
} // namespace shio

#endif // SHIO_CONNECTION_MANAGER_HPP

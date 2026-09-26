/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_POLICY_GENERATOR_HPP
#define CHEFERD_POLICY_GENERATOR_HPP

#include <cheferd/utils/options.hpp>
#include <string>

namespace cheferd {

/**
 * PolicyGenerator class.
 * PolicyGenerator converts housekeeping rules into the "|"-separated string format sent to
 * data plane stages.
 */
class PolicyGenerator {

public:
    /**
     * PolicyGenerator default constructor.
     */
    PolicyGenerator ();

    /**
     * PolicyGenerator default destructor.
     */
    ~PolicyGenerator ();

    /**
     * convert_housekeeping_create_channel_string: Convert HousekeepingCreateChannelRaw object
     * into string format.
     * @param hsk_raw HousekeepingCreateChannelRaw object to be converted into string format.
     * @param hsk_rule Output: rule in string format (previous contents are cleared).
     */
    void convert_housekeeping_create_channel_string (const HousekeepingCreateChannelRaw& hsk_raw,
        std::string& hsk_rule);

    /**
     * convert_housekeeping_create_object_string: Convert HousekeepingCreateObjectRaw object
     * into string format.
     * @param hsk_raw HousekeepingCreateObjectRaw object to be converted into string format.
     * @param hsk_rule Output: rule in string format (previous contents are cleared).
     */
    void convert_housekeeping_create_object_string (const HousekeepingCreateObjectRaw& hsk_raw,
        std::string& hsk_rule);
};

} // namespace cheferd

#endif // CHEFERD_POLICY_GENERATOR_HPP

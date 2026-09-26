/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#ifndef CHEFERD_COMMAND_LINE_PARSER_HPP
#define CHEFERD_COMMAND_LINE_PARSER_HPP

#include <cheferd/utils/options.hpp>
#include <gflags/gflags.h>

namespace cheferd {

/**
 * CommandLineParser class.
 * CommandLineParser processes command line arguments (parsed with gflags).
 * Supported flags:
 * - --config_file: path to the configuration file (default: ../files/core_config_file).
 * Currently, the CommandLineParser class contains the following variables:
 * - config_file_path: path to configuration file.
 */
class CommandLineParser {

public:
    std::string config_file_path;

    /**
     * process_program_options. Process command line arguments.
     * @param argc Number of arguments.
     * @param argv Arguments.
     */
    void process_program_options (int argc, char** argv);

    /**
     * CommandLineParser default constructor.
     */
    CommandLineParser ();

    /**
     * CommandLineParser default destructor.
     */
    ~CommandLineParser ();
};
} // namespace cheferd

#endif // CHEFERD_COMMAND_LINE_PARSER_HPP

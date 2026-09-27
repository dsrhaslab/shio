/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#ifndef SHIO_COMMAND_LINE_PARSER_HPP
#define SHIO_COMMAND_LINE_PARSER_HPP

#include <shio/utils/options.hpp>
#include <gflags/gflags.h>

namespace shio {

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
} // namespace shio

#endif // SHIO_COMMAND_LINE_PARSER_HPP

/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include "shio/utils/command_line_parser.hpp"

#include "shio/utils/logging.hpp"

namespace shio {

DEFINE_string (config_file,
    "../files/core_config_file",
    "Defines the path to the configuration file.");

// CommandLineParser default constructor.
CommandLineParser::CommandLineParser ()
{
    // Logging::log_debug ("CommandLineParser default constructor.");
}

// CommandLineParser default destructor.
CommandLineParser::~CommandLineParser ()
{
    Logging::log_debug ("CommandLineParser default destructor.");
}

// process_program_options call. Process command line arguments.
void CommandLineParser::process_program_options (int argc, char** argv)
{
    // parse flags from stdin
    gflags::ParseCommandLineFlags (&argc, &argv, true);
    config_file_path = fLS::FLAGS_config_file;
}

} // namespace shio

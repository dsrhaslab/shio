/**
 *   Copyright (c) 2026 INESC TEC.
 **/

#include <shio/controller/controller.hpp>
#include <shio/utils/command_line_parser.hpp>
#include <shio/utils/config_file_parser.hpp>
#include <shio/utils/jobs_config_file_parser.hpp>
#include <shio/utils/logging.hpp>
#include <shio/utils/policy_generator.hpp>

using namespace shio;

int main (int argc, char** argv)
{
    Logging logger { shio::option_option_logging_ };
    Logging::log_info ("shio controller starting ...");

    CommandLineParser commandLineParser;

    // Parse and process the command line
    commandLineParser.process_program_options (argc, argv);
    std::string config_file_path = commandLineParser.config_file_path;

    // Parse and process config file
    ConfigFileParser configFileParser;
    configFileParser.process_config_file (config_file_path);

    Controller controller { configFileParser };

    return 0;
}

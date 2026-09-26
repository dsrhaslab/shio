/**
 *   Copyright (c) 2025 INESC TEC.
 **/

#include <cheferd/controller/controller.hpp>
#include <cheferd/utils/command_line_parser.hpp>
#include <cheferd/utils/config_file_parser.hpp>
#include <cheferd/utils/jobs_config_file_parser.hpp>
#include <cheferd/utils/logging.hpp>
#include <cheferd/utils/policy_generator.hpp>

using namespace cheferd;

int main (int argc, char** argv)
{
    Logging logger { cheferd::option_option_logging_ };
    Logging::log_info ("cheferd controller starting ...");

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

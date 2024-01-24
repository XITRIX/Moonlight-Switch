//
// Created by Daniil Vinogradov on 24/01/2024.
//

#include <forwarder_maker.hpp>
#include <Settings.hpp>
#include "nacp.hpp"

void prepareNacp() {
    nacp_tool tool;

    std::string input_filepath = Settings::instance().working_dir() + "/forwarder/control.nacp.xml";
    std::string output_filepath = Settings::instance().working_dir() + "/forwarder/control/control.nacp";

    tool.createnacp(&input_filepath, &output_filepath);
}
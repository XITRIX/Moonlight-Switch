//
//  main_args.hpp
//  Moonlight
//
//  Created by XITRIX on 22.01.2024.
//


#pragma once

#include <string>

void registerDeepLinkHandler();

bool startFromArgs(int argc, char** argv);
bool startFromUrl(const std::string& url, bool resetActivityStack = true);

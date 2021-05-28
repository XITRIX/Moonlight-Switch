//
//  hosts_tabs_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.05.2021.
//

#pragma once

#include <borealis.hpp>
#include <Singleton.hpp>

using namespace brls;

class HostsTabs : public Singleton<HostsTabs>, public brls::TabFrame
{
public:
    HostsTabs();
    void refillTabs();
    static View* create();
};

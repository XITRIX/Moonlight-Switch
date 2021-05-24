/*
    Copyright 2021 XITRIX

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "host_tab.hpp"

#include <borealis/core/i18n.hpp>

using namespace brls::literals;

HostTab::HostTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/host.xml");
    
    connect->setTitle("Wake up");
    remove->setTitle("Remove");
    remove->title->setTextColor(RGB(229, 57, 53));
}

brls::View* HostTab::create()
{
    // Called by the XML engine to create a new ComponentsTab
    return new HostTab();
}

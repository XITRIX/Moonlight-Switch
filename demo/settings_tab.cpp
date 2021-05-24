/*
    Copyright 2021 natinusala

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

#include "settings_tab.hpp"

bool radioSelected = false;

SettingsTab::SettingsTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/settings.xml");


    resolution->init("Resolution", { "720p", "1080p" }, 0, [](int selected) {
        
    });
    
    fps->init("FPS", { "30", "60" }, 1, [](int selected) {

    });
    
    codec->init("Video codec", { "H.264", "HEVC (H.265, Experimental)" }, 0, [](int selected) {

    });
    
    decoder->init("Decoder Threads", { "0 (No use threads)", "2", "3", "4" }, 3, [](int selected) {

    });
    
    slider->setProgress(0.1f);
    slider->getProgressEvent()->subscribe([this](float progress) {
        header->setSubtitle(std::to_string((int)(progress * 149.5 + 0.5)) + ".0 Mbps");
    });
    

    optimal->title->setText("Use Streaming Optimal Playable Settings");
    pcAudio->title->setText("Play Audio on PC");
    writeLog->title->setText("Write log");
}

brls::View* SettingsTab::create()
{
    return new SettingsTab();
}

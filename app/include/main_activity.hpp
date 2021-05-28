//
//  main_activity.hpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#pragma once

#include <borealis.hpp>

class MainActivity : public brls::Activity
{
  public:
    // Declare that the content of this activity is the given XML file
    CONTENT_FROM_XML_RES("activity/main.xml");
};

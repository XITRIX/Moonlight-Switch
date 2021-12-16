//
//  link_cell.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 21.11.2021.
//

#include <borealis.hpp>

class LinkCell : public brls::Box 
{
  public:
    LinkCell();
    static View* create();

    BRLS_BIND(brls::Image, image, "image");
    BRLS_BIND(brls::Label, title, "title");
    BRLS_BIND(brls::Label, subtitle, "subtitle");
    
  private:
    BRLS_BIND(brls::Label, icon, "icon");
};

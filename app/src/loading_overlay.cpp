//
//  loading_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 02.06.2021.
//

#include "loading_overlay.hpp"

const std::string loadingOverlayXML = R"xml(
<brls:Box
    width="auto"
    height="auto"
    axis="column"
    justifyContent="center"
    alignItems="center">

    <brls:ProgressSpinner
        id="progress"
        width="60"
        height="60"/>

</brls:Box>
)xml";

LoadingOverlay::LoadingOverlay(Box* holder)
{
    this->inflateFromXMLString(loadingOverlayXML);
    
    detach();
    holder->addView(this);
    
    this->holder = holder;
    layout();
}

void LoadingOverlay::layout()
{
    if (holder)
    {
        setWidth(holder->getWidth());
        setHeight(holder->getHeight());
    }
}


void LoadingOverlay::setHidden(bool hide)
{
    setAlpha(hide ? 0 : 1);
}

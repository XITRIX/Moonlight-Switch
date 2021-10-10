//
//  mapping_layout_editor.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 09.10.2021.
//

#pragma once

#include <borealis.hpp>
#include <Settings.hpp>

using namespace brls;

class MappingLayoutEditor : public Box
{
public:
    std::function<void(void)> dismissCb;

    MappingLayoutEditor(int layoutNumber, std::function<void(void)> dismissCb);
    ~MappingLayoutEditor();
    
    View* getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction) override;
    void dismiss(std::function<void(void)> cb = []{}) override;
private:
    int layoutNumber;
    Label* titleLabel;

    void renameLayout();
    void removeLayout();
};

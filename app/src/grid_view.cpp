//
//  grid_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.06.2021.
//

#include "grid_view.hpp"

GridView::GridView()
    : Box(Axis::COLUMN)
{
    columls = 7;
}

void GridView::addView(View* view)
{
    if (getChildren().size() % columls == 0)
    {
        if (lastContainer)
            lastContainer->setPaddingBottom(12);
        
        lastContainer = new Box(Axis::ROW);
//        lastContainer->setJustifyContent(JustifyContent::SPACE_BETWEEN);
        Box::addView(lastContainer);
    }
    else if (lastView)
    {
        lastView->setMarginRight(12);
    }
    lastContainer->addView(view);
    children.push_back(view);
    lastView = view;
}

void GridView::clearViews()
{
    Box::clearViews();
    children.clear();
    lastContainer = nullptr;
    lastView = nullptr;
}

View* GridView::getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction)
{
    if (newFocus && (direction == FocusDirection::UP || direction == FocusDirection::DOWN))
    {
        View* source = Application::getCurrentFocus();
        void* currentparentUserData = source->getParentUserData();
        void* nextParentUserData = newFocus->getParentUserData();
        
        size_t currentFocusIndex = *((size_t*)currentparentUserData);
        size_t nextFocusIndex = *((size_t*)nextParentUserData);
        
        if (currentFocusIndex < 0 || currentFocusIndex >= source->getParent()->getChildren().size())
            return Box::getParentNavigationDecision(from, newFocus, direction);
        
        if (newFocus->getParent()->getChildren().size() <= currentFocusIndex)
        {
            newFocus = newFocus->getParent()->getChildren()[newFocus->getParent()->getChildren().size() - 1];
            return Box::getParentNavigationDecision(from, newFocus, direction);
        }
        
        while (newFocus && nextFocusIndex < currentFocusIndex)
        {
            newFocus = newFocus->getNextFocus(FocusDirection::RIGHT, newFocus);
            if (!newFocus)
                break;
            
            nextParentUserData = newFocus->getParentUserData();
            nextFocusIndex = *((size_t*)nextParentUserData);
        }
        
        while (newFocus && nextFocusIndex > currentFocusIndex)
        {
            newFocus = newFocus->getNextFocus(FocusDirection::LEFT, newFocus);
            if (!newFocus)
                break;
            
            nextParentUserData = newFocus->getParentUserData();
            nextFocusIndex = *((size_t*)nextParentUserData);
        }
    }
    return Box::getParentNavigationDecision(from, newFocus, direction);
}

std::vector<View*>& GridView::getChildren()
{
    return this->children;
}

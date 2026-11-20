#pragma once
#ifdef __SWITCH__
#include <borealis.hpp>

class HalfConMappingView : public brls::Box
{
  public:
    HalfConMappingView();
    ~HalfConMappingView() override;
};
#endif

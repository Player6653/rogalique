#include "ArrowCrateComponent.h"
#include <algorithm>

std::vector<ArrowCrateComponent*> ArrowCrateComponent::s_all;

ArrowCrateComponent::ArrowCrateComponent()
{
    s_all.push_back(this);
}

ArrowCrateComponent::~ArrowCrateComponent()
{
    s_all.erase(std::remove(s_all.begin(), s_all.end(), this), s_all.end());
}

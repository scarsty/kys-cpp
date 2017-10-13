#include "UISave.h"

UISave::UISave()
{
}

UISave::~UISave()
{
}

void UISave::onEntrance()
{
    std::vector<std::string> strings;
    for (int i = 1; i <= 10; i++)
    {
        strings.push_back("ßM¶È"+std::to_string(i));
    }
    setStrings(strings);
}

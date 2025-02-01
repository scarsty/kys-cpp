#pragma once
#include "Menu.h"

class UIKeyConfig : public MenuText
{
public:
    struct Keys
    {
        int Left = K_A, Up = K_W, Right = K_D, Down = K_S;
        int Light = K_J, Heavy = K_I, Long = K_K, Slash = K_M;
        int Item = K_P;
    };

    UIKeyConfig();
    virtual ~UIKeyConfig() {};
    virtual void onPressedOK() override;
    virtual void onPressedCancel() override;

    static Keys* getKeyConfig()
    {
        static Keys k;
        return &k;
    }
    static void readFromString(std::string str);
    static std::string toString();

private:
    Keys key_;
    std::string keyToString(int k);
};



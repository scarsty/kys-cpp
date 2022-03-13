#pragma once
#include "Menu.h"

class UIKeyConfig : public MenuText
{
public:
    struct Keys
    {
        int Left = BPK_a, Up = BPK_w, Right = BPK_d, Down = BPK_s;
        int Light = BPK_j, Heavy = BPK_i, Long = BPK_k, Slash = BPK_m;
        int Item = BPK_p;
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



#pragma once
#include "UI.h"

typedef std::function<void(BP_Event&, void*)> ButtonFunc;
#define BIND_FUNC(func) std::bind(&func, this, std::placeholders::_1, std::placeholders::_2)

class Button : public UI
{
public:
    Button();
    virtual ~Button();

    std::string m_strPath;
    int m_nnum1;
    int m_nnum2;
    int m_nnum3;
    void setTexture(const std::string& path, int num1, int num2 = -1, int num3 = -1);
    void draw();
    //void dealEvent(BP_Event& e) override;

    ButtonFunc func = nullptr;
    void setFunction(ButtonFunc func) { this->func = func; }

    int state = 0;

    //BIND_FUNC() { return std::bind(,this, std::placeholders::_1, std::placeholders::_2); }
};


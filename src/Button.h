#pragma once
#include "Base.h"

typedef std::function<void(BP_Event&, void*)> ButtonFunc;
#define BIND_FUNC(func) std::bind(&func, this, std::placeholders::_1, std::placeholders::_2)

class Button : public Base
{
public:
    Button();
    virtual ~Button();

    std::string path;
    int num1;
    int num2;
    int num3;
    void setTexture(const std::string& path, int num1, int num2 = -1, int num3 = -1);

    void draw() override;
    //void dealEvent(BP_Event& e) override;

    ButtonFunc func = nullptr;
    void setFunction(ButtonFunc func) { this->func = func; }

    int state = 0;

    //BIND_FUNC() { return std::bind(,this, std::placeholders::_1, std::placeholders::_2); }
};


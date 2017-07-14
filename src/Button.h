#pragma once
#include "UI.h"

typedef std::function<void()> ButtonFunc;
#define BIND_FUNC(func) std::bind(&func, this)

/**
* @file		 Button.h
* @brief	 按钮类
* @author    xiaowu
* @Remark    20170710 修改底层读图函数 
			 20170714 哇，看了一下这个写的有很多问题啊，比如按钮的状态响应，应该自己去完成啊，而不是用菜单类去完成，菜单类完成的应该是自己的东西

*/



class Button : public UI
{
public:
    Button(const std::string& strNormalpath);
	Button(const std::string& strNormalpath, const std::string& strPasspath, const std::string& strPress);

    virtual ~Button();

	void InitMumber();
    void dealEvent(BP_Event& e) override;
	void draw();

    ButtonFunc func = nullptr;
    void setFunction(ButtonFunc func) { this->func = func; }
private:

	std::string m_strNormalpath, m_strPasspath, m_strPress; //三种状态的按钮图片路径
    int nState; //按钮状态
	enum ButtonState
	{
		Normal,
		Pass,
		Press,
	};

};


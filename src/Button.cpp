#include "Button.h"



Button::Button(const std::string& strNormalpath)
{
	InitMumber();
	m_strNormalpath = strNormalpath;
}

Button::Button(const std::string& strNormalpath, const std::string& strPasspath, const std::string& strPress)
{
	InitMumber();
	m_strNormalpath = strNormalpath;
	m_strPasspath = strPasspath;
	m_strPress = strPress;
}

Button::~Button()
{
}

void Button::InitMumber()
{
	nState = Normal;
	m_strNormalpath = "";
	m_strPasspath = "";
	m_strPress = "";
}

void Button::dealEvent(BP_Event& e)
{
	switch (e.type)
	{
	case BP_MOUSEMOTION:
	{
		nState = Normal;
		if (inSide(e.button.x, e.button.y))
		{
			nState = Pass;
		}
		break;
	}
	case  BP_MOUSEBUTTONDOWN:
	{
		if (e.button.button == BP_BUTTON_LEFT)
		{
			if (inSide(e.button.x, e.button.y))
			{
				nState = Press;
			}
		}
	}
	case BP_MOUSEBUTTONUP:
	{
		if (e.button.button == BP_BUTTON_LEFT)
		{
			if (inSide(e.button.x, e.button.y))
			{
				func();
			}
		}
		if (e.button.button == BP_BUTTON_RIGHT)
		{
			pop();
		}
		break;
	}
		
	}
}

void Button::draw()
{
	std::string strTempPath = m_strNormalpath;

    if (Normal == nState)
    {
		strTempPath = m_strNormalpath;
    }
	else if (Pass == nState && !m_strPasspath.empty())
	{
		strTempPath = m_strPasspath;
	}
	else if(Press == nState && !m_strPress.empty())
	{
		strTempPath = m_strPress;
	}

	Texture::getInstance()->LoadImageByPath(strTempPath, m_nx, m_ny);
}

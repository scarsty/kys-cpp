#include "Button.h"



Button::Button()
{
}


Button::~Button()
{
}

void Button::setTexture(const std::string& path, int num1, int num2 /*= -1*/, int num3 /*= -1*/)
{
    this->m_strPath = path;
    this->m_nnum1 = num1;
    this->m_nnum2 = num2;
    this->m_nnum3 = num3;
}

void Button::draw()
{
    if (state == 0)
    {
        Texture::getInstance()->copyTexture(m_strPath, m_nnum1, m_nx, m_ny);
    }
    else
    {
        if (m_nnum2 >= 0)
        {
            Texture::getInstance()->copyTexture(m_strPath, m_nnum2, m_nx, m_ny);
        }
        else
        {
            Texture::getInstance()->copyTexture(m_strPath, m_nnum1, m_nx + 1, m_ny + 1);
        }
    }
}

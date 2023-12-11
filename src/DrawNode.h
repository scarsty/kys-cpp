#pragma once
#include "RunNode.h"

//辅助绘制的类
class DrawNode : public RunNode
{    
    public:
        virtual ~DrawNode() {}
        virtual void draw() override;
        void clear()
        {
            Infos.clear();
        }
        struct Info
        {
            int type = 0;
            int x = 0, y = 0;
            std::string text;
            int num = 0;
        };
        std::vector<Info> Infos;
};


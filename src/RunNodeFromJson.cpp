#include "RunNodeFromJson.h"
#include "TextBox.h"
#include "filefunc.h"

RunNodeFromJson::RunNodeFromJson(const std::string json_str)
{
    init(json_str);
}

void RunNodeFromJson::init(const std::string json_str)
{
    YAML::Node node;
    if (filefunc::fileExist(json_str))
    {
        node = YAML::LoadFile(json_str);
    }
    else
    {
        node = YAML::Load(json_str);
    }
    auto n = node["Content"]["Content"]["ObjectData"];
    create(n, this);
}

void RunNodeFromJson::create(YAML::Node& n, RunNode* run_node)
{
    std::shared_ptr<RunNode> new_node;

    auto type = n["ctype"].as<std::string>();
    if (type == "TextObjectData")
    {
        auto t = std::make_shared<TextBox>();
        t->setText(n["LabelText"].as<std::string>());
        t->setFontSize(n["FontSize"].as<int>());
        t->setTextPosition(n["Position"]["X"].as<double>(), n["Position"]["Y"].as<double>());
        t->setHaveBox(0);
        BP_Color c{ 255, 255, 255, 255 };
        t->setTextColor(c, c, c);
        new_node = t;
    }
    else if (type == "ImageViewObjectData")
    {
        new_node = std::make_shared<RunNode>();
    }
    else
    {
        new_node = std::make_shared<RunNode>();
    }
    run_node->addChild(new_node);

    fmt1::print("{}\n", n["ctype"].as<std::string>());
    if (n["Children"].IsDefined())
    {
        for (auto& n1 : n["Children"].as<std::vector<YAML::Node>>())
        {
            create(n1, new_node.get());
        }
    }
}

#pragma once
#include "RunNode.h"
#include "yaml-cpp/yaml.h"

class RunNodeFromJson : public RunNode
{
public:
    RunNodeFromJson() {}
    RunNodeFromJson(const std::string json_str);
    void init(const std::string json_str);
    virtual ~RunNodeFromJson() = default;
private:
    void create(YAML::Node& n, RunNode* run_node);
};


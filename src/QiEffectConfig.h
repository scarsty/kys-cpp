#pragma once
#include "QiEffectRenderer.h"
#include <string>
#include <unordered_map>

namespace QiEffect
{

class QiEffectConfig
{
public:
    static QiEffectConfig& instance();

    void ensureLoaded();
    bool isLoaded() const { return loaded_; }

    bool tryGetParams(const Magic* magic, VisualParams& out) const;

private:
    QiEffectConfig() = default;
    bool loadFromDisk();

    bool loaded_ = false;
    VisualParams defaults_;
    std::unordered_map<int, VisualParams> by_magic_id_;
    std::unordered_map<int, VisualParams> by_magic_type_;
};

}  // namespace QiEffect

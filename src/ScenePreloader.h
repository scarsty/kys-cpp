#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ScenePreloader
{

struct LoadProgress
{
    std::size_t completed{};
    std::size_t total = 1;

    double fraction() const
    {
        assert(completed <= total);
        return total == 0 ? 1.0 : static_cast<double>(completed) / total;
    }
};

// 各階段保留固定比重，群組內依逐張貼圖更新；查閱進度不觸發資源 I/O。
class LoadQueue
{
public:
    void add(std::string label, std::function<LoadProgress()> advance, LoadProgress progress = {})
    {
        steps_.push_back({std::move(label), std::move(advance), progress});
        skipCompleted();
    }

    void advance()
    {
        assert(!complete());
        auto& step = steps_[index_];
        const auto progress = step.advance();
        assert(progress.fraction() >= step.progress.fraction());
        step.progress = progress;
        skipCompleted();
    }

    bool complete() const { return index_ == steps_.size(); }
    const std::string& currentLabel() const { assert(!complete()); return steps_[index_].label; }

    double fraction() const
    {
        if (complete()) return 1.0;
        double completed{};
        for (const auto& step : steps_) completed += step.progress.fraction();
        return completed / steps_.size();
    }

private:
    struct Step
    {
        std::string label;
        std::function<LoadProgress()> advance;
        LoadProgress progress;
    };
    std::vector<Step> steps_;
    std::size_t index_{};

    void skipCompleted()
    {
        while (!complete() && steps_[index_].progress.completed == steps_[index_].progress.total) ++index_;
    }
};

void preloadSubSceneAssets(int submapId);
std::vector<std::pair<std::string, int>> battlefieldAssets(int battlefieldId);
void showPromptAndPreload(const std::string& message, const std::function<void()>& preload);

}

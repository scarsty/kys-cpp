#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace KysChess
{

// SDL 的滑鼠雙擊計數搭配穩定控制項身分；觸控沿用相同的 500ms 點擊間隔。
// 成功派送後吞掉同一串連點，避免移位後誤操作鄰格或把剛上陣的棋子撤回。
struct ChessPortraitClicks
{
    std::string key;
    uint64_t lastTimestamp{};
    bool consumed{};

    bool release(const std::string& target, uint8_t clicks, uint64_t timestamp, bool touch, bool singleClick = false)
    {
        const bool close = timestamp >= lastTimestamp && timestamp - lastTimestamp <= 500000000;
        if (consumed && close) { lastTimestamp = timestamp; return false; }
        const bool doubleClick = key == target && close && (touch || clicks == 2);
        key = target;
        lastTimestamp = timestamp;
        consumed = singleClick || doubleClick;
        return consumed;
    }
    void reset() { *this = {}; }
};

// 長按只顯示說明；放開或移出後不執行按鈕，即使移回原位置也不執行。
struct ChessPressPreview
{
    std::string key;
    double startedAt{};
    bool canceled{};

    void start(std::string target, double now) { key = std::move(target); startedAt = now; canceled = false; }
    void move(const std::string& target) { if (target != key) canceled = true; }
    bool visible(double now) const { return !key.empty() && !canceled && now - startedAt >= 350; }
    bool release(double now)
    {
        const bool consumed = !key.empty() && (canceled || visible(now));
        reset();
        return consumed;
    }
    void reset() { *this = {}; }
};

} // namespace KysChess

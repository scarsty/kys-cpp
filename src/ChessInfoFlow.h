#pragma once

#include "ChessUiCommon.h"

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace KysChess
{

template <typename DisplayWidth>
std::string padRightToDisplayWidth(std::string text, int targetWidth, DisplayWidth displayWidth)
{
    while (displayWidth(text) < targetWidth)
    {
        text += ' ';
    }
    return text;
}

template <typename DisplayWidth>
std::vector<std::string> buildAlignedComboCatalogLabels(
    const std::vector<std::pair<std::string, std::string>>& rows,
    DisplayWidth displayWidth)
{
    int nameWidth = 0;
    int countWidth = 0;
    for (const auto& [name, count] : rows)
    {
        nameWidth = std::max(nameWidth, displayWidth(name));
        countWidth = std::max(countWidth, displayWidth(count));
    }

    std::vector<std::string> labels;
    labels.reserve(rows.size());
    for (const auto& [name, count] : rows)
    {
        labels.push_back(std::format(
            "{} ({})",
            padRightToDisplayWidth(name, nameWidth, displayWidth),
            padRightToDisplayWidth(count, countWidth, displayWidth)));
    }
    return labels;
}

class ChessInfoFlow
{
public:
    explicit ChessInfoFlow(const ChessSelectorServices& services);

    void viewChessPool();
    void viewCombos();
    void viewNeigong();
    void showGameGuide();
    void showPositionSwap();

private:
    ChessSelectorServices services_;
};

}    // namespace KysChess

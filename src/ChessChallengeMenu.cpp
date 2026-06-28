#include "ChessChallengeMenu.h"

#include <format>

namespace KysChess
{

ChallengeMenuSetup buildChallengeMenuSetup(
    const std::vector<BalanceConfig::ChallengeDef>& challenges,
    const ChessProgress& progress,
    PanelAnchor menuAnchor)
{
    ChallengeMenuSetup setup;
    for (int i = 0; i < static_cast<int>(challenges.size()); ++i)
    {
        const auto& challenge = challenges[i];
        const bool done = progress.isChallengeCompleted(i);
        setup.data.labels.push_back(std::format("{}{}", done ? "[已通關] " : "", challenge.name));
        setup.data.colors.push_back(done ? Color{120, 120, 120, 255} : Color{255, 200, 100, 255});
    }

    setup.config.perPage = kChallengeMenuItemsPerPage;
    setup.config.x = menuAnchor.x;
    setup.config.y = menuAnchor.y;
    return setup;
}

}    // namespace KysChess

#include "ChessUiCommon.h"

#include "Audio.h"
#include "Engine.h"
#include "Font.h"
#include "TextBox.h"

#include <array>
#include <format>
#include <vector>

namespace KysChess
{

namespace
{

constexpr std::array<int, 20> kChessMusicIds = {0, 1, 2, 8, 9, 13, 14, 16, 18, 19, 20, 21, 23, 24, 25, 31, 59, 60, 61, 62};
constexpr std::array<int, 17> kBattleMusicIds = {3, 4, 5, 6, 7, 10, 17, 48, 53, 55, 70, 75, 79, 80, 84, 88, 90};

template <size_t N>
int pickRandomMusic(const std::array<int, N>& musicIds)
{
    static int lastPlayed = -1;

    int idx = rand() % musicIds.size();
    if (musicIds.size() > 1 && musicIds[idx] == lastPlayed)
    {
        idx = (idx + 1) % musicIds.size();
    }
    lastPlayed = musicIds[idx];
    return musicIds[idx];
}

template <size_t N>
bool containsMusicId(const std::array<int, N>& musicIds, int musicId)
{
    for (int value : musicIds)
    {
        if (value == musicId)
        {
            return true;
        }
    }
    return false;
}

}    // namespace

void showChessMessage(const std::string& text, int fontSize)
{
    auto box = std::make_shared<DismissibleTextBox>();
    box->setText(text);
    box->setFontSize(fontSize);
    box->runCentered(Engine::getInstance()->getUIHeight() / 2);
}

void playChessUpgradeSound()
{
    Audio::getInstance()->playESound(72);
}

int getRandomChessMusic()
{
    return pickRandomMusic(kChessMusicIds);
}

int getRandomBattleMusic()
{
    return pickRandomMusic(kBattleMusicIds);
}

bool isChessSceneMusic(int musicId)
{
    return containsMusicId(kChessMusicIds, musicId);
}

}    // namespace KysChess

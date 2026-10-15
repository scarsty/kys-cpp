#include <Windows.h>

#include <filesystem>
#include <string>

namespace
{

int showWindowsError(const std::wstring& message, DWORD error)
{
    wchar_t details[2048]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, details, static_cast<DWORD>(std::size(details)), nullptr);
    const auto text = message + L"\n\n" + details + L"\nWindows 錯誤碼：" + std::to_wstring(error);
    MessageBoxW(nullptr, text.c_str(), L"金群自走棋", MB_OK | MB_ICONERROR);
    return 1;
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    std::wstring modulePath(256, L'\0');
    for (;;)
    {
        const auto length = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        if (length == 0)
        {
            return showWindowsError(L"無法取得啟動程式的位置。", GetLastError());
        }
        if (length < modulePath.size())
        {
            modulePath.resize(length);
            break;
        }
        modulePath.resize(modulePath.size() * 2);
    }

    const auto packageDirectory = std::filesystem::path(modulePath).parent_path();
    const auto gameExecutable = packageDirectory / L"bin" / L"kys.exe";
    auto commandLine = L"\"" + gameExecutable.native() + L"\" game";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(gameExecutable.c_str(), commandLine.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, packageDirectory.c_str(), &startup, &process))
    {
        const auto error = GetLastError();
        return showWindowsError(
            L"無法啟動遊戲。請先完整解壓縮遊戲資料夾。\n\n遊戲程式：" + gameExecutable.native(),
            error);
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}

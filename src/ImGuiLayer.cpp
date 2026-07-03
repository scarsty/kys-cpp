#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "SDL3/SDL.h"
#include "GameUtil.h"
#include "ImGuiShared.h"
#include <algorithm>
#include <string>
#include <string_view>

using namespace KysImGui;

namespace
{
float clampf(float v, float lo, float hi)
{
    return (std::max)(lo, (std::min)(v, hi));
}

bool isGuardedOverlayEventType(Uint32 type)
{
    switch (type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return true;
    default:
        return false;
    }
}
}

void ImGuiLayer::init(SDL_Window* window, SDL_Renderer* renderer)
{
    if (initialized_ || window == nullptr || renderer == nullptr)
    {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    std::string chinese_font = GameUtil::PATH() + "font/chinese.ttf";
    if (!io.Fonts->AddFontFromFileTTF(chinese_font.c_str(), 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull()))
    {
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();
    ImGui::GetStyle().FontSizeBase = 18.0f;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    initialized_ = true;
}

void ImGuiLayer::shutdown()
{
    if (!initialized_)
    {
        return;
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    initialized_ = false;
    visible_ = false;
}

bool ImGuiLayer::processEvent(const SDL_Event& event)
{
    if (!initialized_)
    {
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT)
    {
        if (battle_log_.model.open && battle_log_.inputGuardFrames <= 0)
        {
            hideBattleLog();
            return true;
        }
        if (changelog_.open && changelog_input_guard_frames_ <= 0)
        {
            hideChangelog();
            return true;
        }
    }

    if (((battle_log_.model.open && battle_log_.inputGuardFrames > 0)
            || (changelog_.open && changelog_input_guard_frames_ > 0))
        && isGuardedOverlayEventType(event.type))
    {
        return true;
    }

    ImGui_ImplSDL3_ProcessEvent(&event);

    if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_F1)
    {
        visible_ = !visible_;
        return true;
    }

    return wantsCaptureEvent(event);
}

void ImGuiLayer::render(SDL_Window* window, SDL_Renderer* renderer, int main_texture_w, int main_texture_h, const char* renderer_name)
{
    if (!initialized_)
    {
        return;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    ImGui::GetStyle().FontScaleMain = clampf(window_h / 720.0f, 1.24f, 3.0f);

    if (!visible_ && !battle_log_.model.open && !changelog_.open && !show_metrics_window_)
    {
        ImGui::EndFrame();
        return;
    }

    if (visible_)
    {
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 180.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Debug Overlay", &visible_))
        {
            ImGui::Text("Press F1 to toggle this overlay.");
            ImGui::Separator();
            ImGui::Text("Renderer: %s", renderer_name ? renderer_name : "unknown");
            ImGui::Text("Window size: %d x %d", window_w, window_h);
            ImGui::Text("Main texture: %d x %d", main_texture_w, main_texture_h);
            ImGui::Text("Frame time: %.3f ms (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Checkbox("Show ImGui metrics window", &show_metrics_window_);
        }
        ImGui::End();
    }

    renderBattleLogWindow();
    renderChangelogWindow();

    if (show_metrics_window_)
    {
        ImGui::ShowMetricsWindow(&show_metrics_window_);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

bool ImGuiLayer::wantsCaptureEvent(const SDL_Event& event) const
{
    if (!initialized_ || (!visible_ && !battle_log_.model.open && !changelog_.open))
    {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    switch (event.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
        return battle_log_.model.open || changelog_.open || io.WantCaptureMouse;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return battle_log_.model.open || changelog_.open || io.WantCaptureKeyboard || io.WantTextInput;
    default:
        return false;
    }
}

void ImGuiLayer::showBattleLog(const BattleLogViewModel& model)
{
    battle_log_view_.show(battle_log_, model);
}

void ImGuiLayer::hideBattleLog()
{
    battle_log_view_.hide(battle_log_);
}

bool ImGuiLayer::isBattleLogOpen() const
{
    return battle_log_view_.isOpen(battle_log_);
}

void ImGuiLayer::showChangelog(const ChangelogData& data)
{
    changelog_ = data;
    changelog_.open = true;
    changelog_input_guard_frames_ = 8;
    changelog_hover_guard_ = true;
    changelog_dragging_ = false;
}

void ImGuiLayer::hideChangelog()
{
    changelog_.open = false;
    changelog_input_guard_frames_ = 0;
    changelog_hover_guard_ = false;
    changelog_dragging_ = false;
}

bool ImGuiLayer::isChangelogOpen() const
{
    return changelog_.open;
}

void ImGuiLayer::renderBattleLogWindow()
{
    battle_log_view_.render(battle_log_);
}

void ImGuiLayer::renderChangelogWindow()
{
    if (!changelog_.open)
    {
        return;
    }

    const ImVec4 panel_bg = colorU8(31, 34, 29, 232);
    const ImVec4 panel_border = colorU8(190, 166, 96, 205);
    const ImVec4 title_gold = colorU8(242, 222, 126);
    const ImVec4 heading_gold = colorU8(232, 205, 112);
    const ImVec4 text_main = colorU8(236, 232, 215);
    const ImVec4 text_muted = colorU8(168, 164, 145);
    const ImVec4 bullet_color = colorU8(211, 177, 92);
    const ImVec4 accent_line = colorU8(166, 145, 78, 90);
    const ImVec4 chip_bg = colorU8(91, 96, 59, 165);
    const ImVec4 child_bg = colorU8(18, 21, 18, 118);
    const ImVec4 dim_bg = colorU8(6, 7, 6, 70);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 vp_pos = viewport ? viewport->Pos : ImVec2(0.0f, 0.0f);
    ImVec2 vp_size = viewport ? viewport->Size : ImVec2(1280.0f, 720.0f);
    const ImGuiIO& io = ImGui::GetIO();
    if (changelog_hover_guard_ && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
    {
        changelog_hover_guard_ = false;
    }

    const float fs = ImGui::GetFontSize();
    const float fsBase = ImGui::GetStyle().FontSizeBase;
    const bool allow_close = changelog_input_guard_frames_ <= 0;
    if (allow_close && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        changelog_.open = false;
    }

    ImVec2 panel_size((std::max)(fs * 36.0f, vp_size.x * 0.72f), (std::max)(fs * 24.0f, vp_size.y * 0.88f));
    panel_size.x = (std::min)(panel_size.x, vp_size.x - fs * 0.8f);
    panel_size.y = (std::min)(panel_size.y, vp_size.y - fs * 0.6f);
    ImVec2 panel_pos(vp_pos.x + (vp_size.x - panel_size.x) * 0.5f, vp_pos.y + (vp_size.y - panel_size.y) * 0.5f);
    bool suppress_hover = changelog_hover_guard_ || !allow_close;

    ImGui::GetForegroundDrawList()->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y), ImGui::ColorConvertFloat4ToU32(dim_bg));

    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, fs * 0.7f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(fs * 1.1f, fs * 0.9f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, fs * 0.45f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, fs * 0.25f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, fs * 1.4f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, panel_border);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, child_bg);
    ImGui::PushStyleColor(ImGuiCol_Separator, accent_line);

    if (ImGui::Begin("##changelog_window", &changelog_.open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        if (suppress_hover)
        {
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chip_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, chip_bg);
        }

        ImGui::PushFont(nullptr, fsBase * 1.35f);
        ImGui::PushStyleColor(ImGuiCol_Text, title_gold);
        textLocalized(changelog_.title.empty() ? "更新日誌" : changelog_.title);
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float button_h = ImGui::GetFrameHeight() * 1.45f;
        const float footer_h = button_h + fs * 1.15f;
        ImVec2 child_size(ImGui::GetContentRegionAvail().x, (std::max)(0.0f, ImGui::GetContentRegionAvail().y - footer_h));
        if (child_size.x < 1.0f) child_size.x = 1.0f;
        if (child_size.y < 1.0f) child_size.y = 1.0f;
        ImVec2 child_pos = ImGui::GetCursorScreenPos();
        ImGui::BeginChild("changelog_body", child_size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

        if (!changelog_.error.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, colorU8(255, 170, 120));
            auto error = localizeImGuiText(changelog_.error);
            ImGui::TextWrapped("%s", error.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            for (const auto& line : changelog_.lines)
            {
                if (line.text.empty())
                {
                    ImGui::Spacing();
                    continue;
                }

                if (line.headingLevel > 0)
                {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, heading_gold);
                    ImGui::PushFont(nullptr, line.headingLevel == 1 ? fsBase * 1.22f : fsBase * 1.12f);
                    auto heading = localizeImGuiText(line.text);
                    ImGui::TextWrapped("%s", heading.c_str());
                    ImGui::PopFont();
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    continue;
                }

                if (line.bulletIndent >= 0)
                {
                    const float indent = fs * (0.75f + line.bulletIndent * 1.25f);
                    ImGui::Indent(indent);
                    ImGui::PushStyleColor(ImGuiCol_Text, bullet_color);
                    ImGui::Bullet();
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, text_main);
                    auto bullet = localizeImGuiText(line.text);
                    ImGui::TextWrapped("%s", bullet.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Unindent(indent);
                    continue;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, text_main);
                auto text = localizeImGuiText(line.text);
                ImGui::TextWrapped("%s", text.c_str());
                ImGui::PopStyleColor();
            }
        }

        const ImGuiIO& drag_io = ImGui::GetIO();
        bool mouse_in_child = drag_io.MousePos.x >= child_pos.x
            && drag_io.MousePos.x < child_pos.x + child_size.x
            && drag_io.MousePos.y >= child_pos.y
            && drag_io.MousePos.y < child_pos.y + child_size.y;
        if (allow_close && !changelog_dragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mouse_in_child)
        {
            changelog_dragging_ = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            changelog_dragging_ = false;
        }
        if (changelog_dragging_)
        {
            ImGui::SetScrollY(ImGui::GetScrollY() - drag_io.MouseDelta.y);
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_w = fs * 8.0f;
        float button_x = ImGui::GetWindowContentRegionMax().x - button_w;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), button_x));
        ImGui::PushStyleColor(ImGuiCol_Button, chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, allow_close ? colorU8(67, 78, 39, 230) : chip_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, allow_close ? colorU8(88, 98, 50, 230) : chip_bg);
        if (buttonLocalized("返回", ImVec2(button_w, button_h)) && allow_close)
        {
            changelog_.open = false;
            changelog_dragging_ = false;
        }
        ImGui::PopStyleColor(3);

        if (suppress_hover)
        {
            ImGui::PopStyleColor(2);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(6);

    if (!changelog_.open)
    {
        changelog_input_guard_frames_ = 0;
        changelog_hover_guard_ = false;
        changelog_dragging_ = false;
    }
    else if (changelog_input_guard_frames_ > 0)
    {
        changelog_input_guard_frames_--;
    }
}

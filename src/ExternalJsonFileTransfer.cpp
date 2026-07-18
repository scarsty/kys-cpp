#include "ExternalJsonFileTransfer.h"

#include "Engine.h"
#include "Font.h"
#include "RunNode.h"

#include <SDL3/SDL.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace
{

#ifdef __EMSCRIPTEN__

constexpr int kDialogPollMs = 16;

void openWebDialog(
    const std::string& title,
    const std::string& text,
    bool importMode,
    const std::string& filename)
{
    EM_ASM({
        if (Module.kysOpenExternalSaveDialog) {
            Module.kysOpenExternalSaveDialog(
                UTF8ToString($0),
                UTF8ToString($1),
                !!$2,
                UTF8ToString($3));
        }
    }, title.c_str(), text.c_str(), importMode ? 1 : 0, filename.c_str());
}

int pollWebDialog()
{
    return EM_ASM_INT({
        return Module.kysPollExternalSaveDialog ? Module.kysPollExternalSaveDialog() : 2;
    });
}

std::string takeWebDialogText()
{
    const int length = EM_ASM_INT({
        return Module.kysGetExternalSaveDialogTextLength
            ? Module.kysGetExternalSaveDialogTextLength()
            : 0;
    });
    std::vector<char> buffer(static_cast<std::size_t>(length) + 1, '\0');
    EM_ASM({
        if (Module.kysWriteExternalSaveDialogTextToBuffer) {
            Module.kysWriteExternalSaveDialogTextToBuffer($0, $1);
        }
    }, buffer.data(), static_cast<int>(buffer.size()));
    return std::string(buffer.data());
}

void closeWebDialog()
{
    EM_ASM({
        if (Module.kysCloseExternalSaveDialog) {
            Module.kysCloseExternalSaveDialog();
        }
    });
}

ExternalJsonTransferResult runWebDialog(
    std::string title,
    std::string text,
    bool importMode,
    std::string filename)
{
    openWebDialog(title, text, importMode, filename);
    for (;;)
    {
        const int state = pollWebDialog();
        if (state == 0)
        {
            emscripten_sleep(kDialogPollMs);
            continue;
        }
        if (state == 1)
        {
            ExternalJsonTransferResult result;
            result.status = ExternalJsonTransferStatus::Completed;
            result.text = takeWebDialogText();
            closeWebDialog();
            return result;
        }
        closeWebDialog();
        return {};
    }
}

#else

struct NativeDialogState
{
    std::mutex mutex;
    bool finished = false;
    ExternalJsonTransferResult result;
};

struct NativeDialogContext
{
    std::shared_ptr<NativeDialogState> state;
    std::string exportText;
    bool importMode = false;
};

void finishNativeDialog(
    const std::shared_ptr<NativeDialogState>& state,
    ExternalJsonTransferResult result)
{
    std::scoped_lock lock(state->mutex);
    state->result = std::move(result);
    state->finished = true;
}

void SDLCALL nativeDialogCallback(
    void* userdata,
    const char* const* filelist,
    int)
{
    std::unique_ptr<NativeDialogContext> context(
        static_cast<NativeDialogContext*>(userdata));
    ExternalJsonTransferResult result;
    if (!filelist)
    {
        result.status = ExternalJsonTransferStatus::Error;
        result.error = SDL_GetError();
        finishNativeDialog(context->state, std::move(result));
        return;
    }
    if (!filelist[0])
    {
        finishNativeDialog(context->state, std::move(result));
        return;
    }

    if (context->importMode)
    {
        std::size_t size{};
        void* bytes = SDL_LoadFile(filelist[0], &size);
        if (!bytes)
        {
            result.status = ExternalJsonTransferStatus::Error;
            result.error = SDL_GetError();
        }
        else
        {
            result.status = ExternalJsonTransferStatus::Completed;
            result.text.assign(static_cast<const char*>(bytes), size);
            SDL_free(bytes);
        }
        finishNativeDialog(context->state, std::move(result));
        return;
    }

    SDL_IOStream* output = SDL_IOFromFile(filelist[0], "wb");
    if (!output)
    {
        result.status = ExternalJsonTransferStatus::Error;
        result.error = SDL_GetError();
    }
    else
    {
        const auto written = SDL_WriteIO(
            output,
            context->exportText.data(),
            context->exportText.size());
        const bool closed = SDL_CloseIO(output);
        if (written != context->exportText.size() || !closed)
        {
            result.status = ExternalJsonTransferStatus::Error;
            result.error = SDL_GetError();
        }
        else
        {
            result.status = ExternalJsonTransferStatus::Completed;
        }
    }
    finishNativeDialog(context->state, std::move(result));
}

class NativeDialogWaitNode : public RunNode
{
public:
    NativeDialogWaitNode(
        std::shared_ptr<NativeDialogState> state,
        std::string title)
        : state_(std::move(state)), title_(std::move(title))
    {
        dark_ = 1;
    }

    void draw() override
    {
        auto* engine = Engine::getInstance();
        const int width = engine->getUIWidth();
        const int height = engine->getUIHeight();
        engine->fillRoundedRect(
            {12, 16, 22, 245},
            width / 2 - 330,
            height / 2 - 85,
            660,
            170,
            12);
        engine->drawRoundedRect(
            {150, 160, 165, 240},
            width / 2 - 330,
            height / 2 - 85,
            660,
            170,
            12);
        Font::getInstance()->drawWithBoxCentered(title_, 32, height / 2 - 42);
        Font::getInstance()->drawWithBoxCentered("等待系統檔案選擇器...", 24, height / 2 + 8);
    }

    void backRun() override
    {
        std::scoped_lock lock(state_->mutex);
        if (state_->finished)
        {
            setExit(true);
        }
    }

    void onPressedCancel() override {}

private:
    std::shared_ptr<NativeDialogState> state_;
    std::string title_;
};

ExternalJsonTransferResult runNativeDialog(
    std::string title,
    std::string filename,
    std::string text,
    bool importMode)
{
    auto state = std::make_shared<NativeDialogState>();
    auto* context = new NativeDialogContext{
        state,
        std::move(text),
        importMode,
    };
    static const SDL_DialogFileFilter filters[]{
        {"JSON", "json"},
    };
    if (importMode)
    {
        SDL_ShowOpenFileDialog(
            nativeDialogCallback,
            context,
            Engine::getInstance()->getWindow(),
            filters,
            1,
            nullptr,
            false);
    }
    else
    {
        SDL_ShowSaveFileDialog(
            nativeDialogCallback,
            context,
            Engine::getInstance()->getWindow(),
            filters,
            1,
            filename.c_str());
    }

    auto wait = std::make_shared<NativeDialogWaitNode>(state, std::move(title));
    wait->run();
    std::scoped_lock lock(state->mutex);
    return std::move(state->result);
}

#endif

}

ExternalJsonTransferResult ExternalJsonFileTransfer::importJson(std::string_view title)
{
#ifdef __EMSCRIPTEN__
    return runWebDialog(std::string(title), {}, true, {});
#else
    return runNativeDialog(std::string(title), {}, {}, true);
#endif
}

ExternalJsonTransferResult ExternalJsonFileTransfer::exportJson(
    std::string_view title,
    std::string_view suggestedFilename,
    std::string_view text)
{
#ifdef __EMSCRIPTEN__
    return runWebDialog(
        std::string(title),
        std::string(text),
        false,
        std::string(suggestedFilename));
#else
    return runNativeDialog(
        std::string(title),
        std::string(suggestedFilename),
        std::string(text),
        false);
#endif
}

#include "McpServer.h"
#include "Engine.h"
#include "BattleScene.h"
#include "InputBox.h"
#include "Menu.h"
#include "MainScene.h"
#include "Save.h"
#include "SubScene.h"
#include "Talk.h"
#include "TitleScene.h"
#include <SDL3_image/SDL_image.h>
#include <glaze/glaze.hpp>
#include <array>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace
{
using Json = glz::generic_json<glz::num_mode::i64>;
constexpr size_t max_line = 65536;

Json object() { return Json::object_t{}; }
Json array() { return Json::array_t{}; }
const Json* field(const Json& value, const char* name)
{
    if (!value.is_object()) { return nullptr; }
    auto& fields = value.get_object();
    auto it = fields.find(name);
    return it == fields.end() ? nullptr : &it->second;
}
std::string text(const Json* value)
{
    return value && value->is_string() ? value->get<std::string>() : "";
}
std::string encode(const Json& value) { return glz::write_json(value).value(); }
Json response(const Json& id, const Json& result)
{
    auto value = object();
    value["jsonrpc"] = "2.0";
    value["id"] = id;
    value["result"] = result;
    return value;
}
Json error(const Json& id, int code, const std::string& message)
{
    auto value = object();
    value["jsonrpc"] = "2.0";
    value["id"] = id;
    value["error"]["code"] = code;
    value["error"]["message"] = message;
    return value;
}
Json toolResult(const Json& result, bool failed = false)
{
    auto value = object();
    auto content = object();
    content["type"] = "text";
    content["text"] = encode(result);
    value["content"] = Json::array_t{content};
    value["isError"] = failed;
    return value;
}
Json failure(const std::string& message)
{
    auto value = object();
    value["error"] = message;
    return toolResult(value, true);
}
std::string nodeType(RunNode* node)
{
    if (dynamic_cast<InputBox*>(node)) { return "input"; }
    if (dynamic_cast<Talk*>(node)) { return "dialogue"; }
    if (dynamic_cast<Menu*>(node)) { return "menu"; }
    if (dynamic_cast<BattleScene*>(node)) { return "battle"; }
    if (dynamic_cast<SubScene*>(node)) { return "submap"; }
    if (dynamic_cast<MainScene*>(node)) { return "world"; }
    if (dynamic_cast<TitleScene*>(node)) { return "title"; }
    return "node";
}
Json describe(RunNode* node, int depth = 0)
{
    auto value = object();
    value["type"] = nodeType(node);
    value["visible"] = node->getVisible();
    value["selected"] = node->getActiveChildIndex();
    if (auto talk = dynamic_cast<Talk*>(node)) { value["text"] = talk->getContent(); }
    if (auto box = dynamic_cast<TextBox*>(node)) { value["text"] = box->getText(); }
    value["children"] = array();
    if (depth < 4)
    {
        int count = 0;
        for (auto& child : node->getChilds())
        {
            if (++count > 64) { break; }
            if (child->getVisible()) { value["children"].get_array().push_back(describe(child.get(), depth + 1)); }
        }
    }
    return value;
}
Json tools()
{
    auto result = object();
    result["tools"] = array();
    const std::array definitions = {
        std::pair{"get_game_state", "Read current scene, committed player position and input context."},
        std::pair{"get_party", "Read the current party. Empty before a game is loaded."},
        std::pair{"get_inventory", "Read nonempty inventory slots. Empty before a game is loaded."},
        std::pair{"get_ui_state", "Read the current input node, dialogue and visible child controls."},
        std::pair{"press_key", "Queue one discrete UI key press, not a held key. Accepted does not mean completed. Disabled during animations and in battle."},
        std::pair{"capture_screen", "Capture the next fully composed window frame including UI as PNG."}
    };
    for (auto [name, description] : definitions)
    {
        auto tool = object();
        tool["name"] = name;
        tool["description"] = description;
        auto schema = object();
        schema["type"] = "object";
        schema["properties"] = object();
        schema["additionalProperties"] = false;
        if (std::string_view(name) == "press_key")
        {
            schema["properties"]["key"]["type"] = "string";
            schema["properties"]["key"]["enum"] = Json::array_t{"up", "down", "left", "right", "confirm", "cancel", "page_up", "page_down"};
            schema["required"] = Json::array_t{"key"};
        }
        tool["inputSchema"] = schema;
        tool["annotations"]["readOnlyHint"] = std::string_view(name) != "press_key";
        tool["annotations"]["destructiveHint"] = std::string_view(name) == "press_key";
        tool["annotations"]["openWorldHint"] = false;
        result["tools"].get_array().push_back(std::move(tool));
    }
    return result;
}
std::string base64(const unsigned char* bytes, size_t size)
{
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((size + 2) / 3 * 4);
    for (size_t i = 0; i < size; i += 3)
    {
        unsigned n = unsigned(bytes[i]) << 16;
        if (i + 1 < size) { n |= unsigned(bytes[i + 1]) << 8; }
        if (i + 2 < size) { n |= bytes[i + 2]; }
        result += alphabet[(n >> 18) & 63];
        result += alphabet[(n >> 12) & 63];
        result += i + 1 < size ? alphabet[(n >> 6) & 63] : '=';
        result += i + 2 < size ? alphabet[n & 63] : '=';
    }
    return result;
}
}

McpServer& McpServer::instance()
{
    static McpServer server;
    return server;
}

bool McpServer::start()
{
#ifdef _WIN32
    int protocol = _dup(_fileno(stdout));
    if (protocol < 0) { return false; }
    if (_dup2(_fileno(stderr), _fileno(stdout)) != 0) { _close(protocol); return false; }
    _setmode(protocol, _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
    output_ = _fdopen(protocol, "wb");
#else
    int protocol = dup(fileno(stdout));
    if (protocol < 0) { return false; }
    if (dup2(fileno(stderr), fileno(stdout)) < 0) { close(protocol); return false; }
    output_ = fdopen(protocol, "w");
#endif
    return output_ != nullptr;
}

void McpServer::send(const std::string& message)
{
    if (disconnected_) { return; }
    if (fwrite(message.data(), 1, message.size(), output_) != message.size()
        || fputc('\n', output_) == EOF || fflush(output_) != 0)
    {
        disconnected_ = true;
    }
}

void McpServer::pump(RunNode* current, bool interactive)
{
    if (!enabled()) { return; }
    if (!disconnected_ && !input_closed_)
    {
        std::array<char, 4096> buffer;
        for (int batch = 0; batch < 16; ++batch)
        {
            int count = 0;
#ifdef _WIN32
            auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdin)));
            DWORD available = 0;
            if (GetFileType(handle) == FILE_TYPE_PIPE)
            {
                if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) { input_closed_ = true; break; }
                if (!available) { break; }
            }
            else if (GetFileType(handle) != FILE_TYPE_DISK) { disconnected_ = true; break; }
            else { available = DWORD(buffer.size()); }
            count = _read(_fileno(stdin), buffer.data(), unsigned(std::min<size_t>(available, buffer.size())));
#else
            pollfd descriptor{fileno(stdin), POLLIN, 0};
            if (poll(&descriptor, 1, 0) <= 0) { break; }
            count = int(read(fileno(stdin), buffer.data(), buffer.size()));
#endif
            if (count <= 0) { input_closed_ = true; break; }
            for (int i = 0; i < count; ++i)
            {
                char c = buffer[i];
                if (c == '\n')
                {
                    if (discarding_) { send(encode(error(nullptr, -32700, "Message exceeds 64 KiB"))); }
                    else if (!input_.empty())
                    {
                        if (requests_.size() < 64) { requests_.push_back(std::move(input_)); }
                        else { disconnected_ = true; }
                    }
                    input_.clear();
                    discarding_ = false;
                }
                else if (!discarding_)
                {
                    input_ += c;
                    if (input_.size() > max_line) { input_.clear(); discarding_ = true; }
                }
            }
        }
    }
    if (disconnected_ || (input_closed_ && requests_.empty() && capture_id_.empty()))
    {
        pending_key_ = 0;
        RunNode::exitAll();
        current->setExit(true);
        return;
    }
    for (int i = 0; i < 8 && !requests_.empty(); ++i)
    {
        auto message = std::move(requests_.front());
        requests_.pop_front();
        try { handle(message, current, interactive); }
        catch (const std::exception& e)
        {
            fprintf(stderr, "MCP request failed: %s\n", e.what());
            Json request;
            if (!glz::read_json(request, message))
            {
                if (auto id = field(request, "id")) { send(encode(error(*id, -32603, "Internal error"))); }
            }
        }
    }
}

void McpServer::handle(const std::string& message, RunNode* current, bool interactive)
{
    Json request;
    if (glz::read_json(request, message)) { send(encode(error(nullptr, -32700, "Parse error"))); return; }
    auto id_ptr = field(request, "id");
    Json id = id_ptr ? *id_ptr : Json{};
    auto method = text(field(request, "method"));
    if (!request.is_object() || text(field(request, "jsonrpc")) != "2.0" || method.empty()
        || (id_ptr && !id.is_string() && !id.get_if<int64_t>()))
    {
        send(encode(error(nullptr, -32600, "Invalid Request")));
        return;
    }
    auto params_ptr = field(request, "params");
    Json params = params_ptr ? *params_ptr : object();
    if (params_ptr && !params.is_object())
    {
        if (id_ptr) { send(encode(error(id, -32602, "Expected object parameters"))); }
        return;
    }
    if (!id_ptr)
    {
        if (method == "notifications/initialized" && initialized_) { ready_ = true; }
        return;
    }
    if (method == "initialize")
    {
        auto version = text(field(params, "protocolVersion"));
        if (initialized_ || version.empty() || !field(params, "capabilities") || !field(params, "clientInfo"))
        {
            send(encode(error(id, -32602, "Invalid initialization"))); return;
        }
        auto result = object();
        result["protocolVersion"] = version == "2024-11-05" ? version : "2025-06-18";
        result["capabilities"]["tools"] = object();
        result["serverInfo"]["name"] = "kys-cpp";
        result["serverInfo"]["version"] = "1.0.0";
        result["instructions"] = "This server owns a visible game process. Use get_ui_state before discrete UI key presses. Closing stdin closes the game. No arbitrary scripts or file access.";
        initialized_ = true;
        send(encode(response(id, result)));
        return;
    }
    if (method == "ping") { send(encode(response(id, object()))); return; }
    if (!ready_) { send(encode(error(id, -32000, "Initialize the MCP session first"))); return; }
    if (method == "tools/list") { send(encode(response(id, tools()))); return; }
    if (method != "tools/call") { send(encode(error(id, -32601, "Method not found"))); return; }
    auto name = text(field(params, "name"));
    auto args_ptr = field(params, "arguments");
    Json args = args_ptr ? *args_ptr : object();
    if (!args.is_object()) { send(encode(error(id, -32602, "Expected object arguments"))); return; }
    if (name != "get_game_state" && name != "get_party" && name != "get_inventory" && name != "get_ui_state"
        && name != "press_key" && name != "capture_screen")
    {
        send(encode(error(id, -32602, "Unknown tool"))); return;
    }
    if (args.get_object().size() != (name == "press_key" ? 1 : 0))
    {
        send(encode(error(id, -32602, "Unexpected tool arguments"))); return;
    }
    auto result = object();
    auto scene = RunNode::getPointerFromRoot<Scene>();
    auto battle = RunNode::getPointerFromRoot<BattleScene>();
    if (name == "get_game_state")
    {
        result["context"] = nodeType(current);
        result["interactive"] = interactive;
        result["loaded"] = !Save::getInstance()->getRoles().empty();
        result["battle"] = bool(battle);
        if (scene)
        {
            result["scene"] = nodeType(scene.get());
            if (!battle)
            {
                result["x"] = scene->man_x_;
                result["y"] = scene->man_y_;
                result["walking"] = scene->isWalking();
            }
            if (auto sub = dynamic_cast<SubScene*>(scene.get())) { result["map_id"] = sub->submap_id_; }
        }
    }
    else if (name == "get_ui_state") { result = describe(current); }
    else if (name == "get_party")
    {
        result["members"] = array();
        auto save = Save::getInstance();
        if (!save->getRoles().empty())
        {
            for (int member_id : save->Team)
            {
                auto role = save->getRole(member_id);
                if (!role) { continue; }
                auto member = object();
                member["id"] = role->ID;
                member["name"] = role->Name;
                member["level"] = role->Level;
                member["hp"] = role->HP;
                member["max_hp"] = role->MaxHP;
                member["mp"] = role->MP;
                member["max_mp"] = role->MaxMP;
                result["members"].get_array().push_back(std::move(member));
            }
        }
    }
    else if (name == "get_inventory")
    {
        result["items"] = array();
        auto save = Save::getInstance();
        if (!save->getRoles().empty())
        {
            for (const auto& slot : save->Items)
            {
                auto item = save->getItem(slot.ItemID);
                if (!item || slot.Count <= 0) { continue; }
                auto entry = object();
                entry["id"] = slot.ItemID;
                entry["name"] = item->Name;
                entry["count"] = slot.Count;
                result["items"].get_array().push_back(std::move(entry));
            }
        }
    }
    else if (name == "press_key")
    {
        const std::array keys = {std::pair{"up", K_UP}, std::pair{"down", K_DOWN}, std::pair{"left", K_LEFT},
            std::pair{"right", K_RIGHT}, std::pair{"confirm", K_RETURN}, std::pair{"cancel", K_ESCAPE},
            std::pair{"page_up", K_PAGEUP}, std::pair{"page_down", K_PAGEDOWN}};
        auto key = text(field(args, "key"));
        int code = 0;
        for (auto [label, value] : keys) { if (key == label) { code = int(value); } }
        if (!code) { send(encode(error(id, -32602, "Unknown key"))); return; }
        if (!interactive || battle || pending_key_)
        {
            send(encode(response(id, failure("Input unavailable during animation, battle or a pending press")))); return;
        }
        pending_key_ = code;
        pending_down_ = true;
        result["accepted"] = true;
    }
    else if (name == "capture_screen")
    {
        if (!capture_id_.empty()) { send(encode(response(id, failure("A capture is already pending")))); return; }
        capture_id_ = encode(id);
        return;
    }
    send(encode(response(id, toolResult(result))));
}

bool McpServer::takeEvent(SDL_Event& event)
{
    if (!pending_key_) { return false; }
    event = {};
    event.type = pending_down_ ? EVENT_KEY_DOWN : EVENT_KEY_UP;
    event.key.key = pending_key_;
    event.key.down = pending_down_;
    if (!pending_down_) { pending_key_ = 0; }
    pending_down_ = false;
    return true;
}

void McpServer::captureFrame()
{
    if (capture_id_.empty()) { return; }
    Json id;
    (void)glz::read_json(id, capture_id_);
    capture_id_.clear();
    auto surface = SDL_RenderReadPixels(Engine::getInstance()->getRenderer(), nullptr);
    auto stream = SDL_IOFromDynamicMem();
    if (!surface || !stream || !IMG_SavePNG_IO(surface, stream, false))
    {
        if (surface) { SDL_DestroySurface(surface); }
        if (stream) { SDL_CloseIO(stream); }
        send(encode(response(id, failure("Unable to capture window")))); return;
    }
    auto bytes = static_cast<const unsigned char*>(SDL_GetPointerProperty(SDL_GetIOProperties(stream), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr));
    auto size = SDL_GetIOSize(stream);
    if (!bytes || size < 0 || size > 16 * 1024 * 1024)
    {
        SDL_DestroySurface(surface);
        SDL_CloseIO(stream);
        send(encode(response(id, failure("Captured image exceeds 16 MiB")))); return;
    }
    auto image = object();
    image["type"] = "image";
    image["mimeType"] = "image/png";
    image["data"] = base64(bytes, size_t(size));
    auto result = object();
    result["content"] = Json::array_t{std::move(image)};
    SDL_DestroySurface(surface);
    SDL_CloseIO(stream);
    send(encode(response(id, result)));
}

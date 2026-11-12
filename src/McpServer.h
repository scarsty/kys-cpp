#pragma once
#include <cstdio>
#include <deque>
#include <glaze/glaze.hpp>
#include <string>

class RunNode;
union SDL_Event;

class McpServer
{
public:
    static McpServer& instance();
    bool start();
    bool enabled() const { return output_ != nullptr; }
    void pump(RunNode* current, bool interactive);
    bool takeEvent(SDL_Event& event);
    void captureFrame();

private:
    using Json = glz::generic_json<glz::num_mode::i64>;
    static constexpr size_t max_line = 65536;
    static Json object();
    static Json array();
    static const Json* field(const Json& value, const char* name);
    static std::string text(const Json* value);
    static std::string encode(const Json& value);
    static Json response(const Json& id, const Json& result);
    static Json error(const Json& id, int code, const std::string& message);
    static Json toolResult(const Json& result, bool failed = false);
    static Json failure(const std::string& message);
    static std::string nodeType(RunNode* node);
    static Json describe(RunNode* node, int depth = 0);
    static Json tools();
    static std::string base64(const unsigned char* bytes, size_t size);
    FILE* output_ = nullptr;
    std::string input_;
    bool discarding_ = false;
    bool disconnected_ = false;
    bool input_closed_ = false;
    bool initialized_ = false;
    bool ready_ = false;
    int pending_key_ = 0;
    bool pending_down_ = false;
    int pending_mouse_button_ = 0;
    int pending_mouse_x_ = 0, pending_mouse_y_ = 0;
    int pending_mouse_phase_ = 0;
    std::deque<std::string> requests_;
    std::string capture_id_;

    void send(const std::string& message);
    void handle(const std::string& message, RunNode* current, bool interactive);
};

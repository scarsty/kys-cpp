#pragma once
#include <cstdio>
#include <deque>
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
    FILE* output_ = nullptr;
    std::string input_;
    bool discarding_ = false;
    bool disconnected_ = false;
    bool input_closed_ = false;
    bool initialized_ = false;
    bool ready_ = false;
    int pending_key_ = 0;
    bool pending_down_ = false;
    std::deque<std::string> requests_;
    std::string capture_id_;

    void send(const std::string& message);
    void handle(const std::string& message, RunNode* current, bool interactive);
};

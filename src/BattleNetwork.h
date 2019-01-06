#pragma once

#ifdef _NETWORK
#include "asio.hpp"
#endif

#include "BattleMenu.h"
#include "GameUtil.h"

#include <array>
#include <memory>
#include <thread>
#include <system_error>

// 使用方法
// 1. 选择人物
// 2. 创建BattleNetwork
// 3. 设置最终回调，和seed等数据
// 4. 在某非阻塞ui中调用handshake
// 5. handshake结束后回调，关闭ui组件

#ifdef _NETWORK
class BattleNetwork
{
public:
    static const std::size_t VALSIZE = 32;

    // , strand_(io_context_)
    BattleNetwork(const std::string& strID, const std::string& port)
        : socket_(io_context_)
        , resolver_(io_context_)
        , work_(io_context_)
        , strID_(strID)
        , port_(port)
        , query_(GameUtil::getInstance()->getString("network", "server", "138.197.200.52"), port_) {};

    virtual ~BattleNetwork()
    {
        io_context_.stop();
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    void asyncRun()
    {
        worker_ = std::thread([this]
        {
            printf("spawning thread\n");
            io_context_.run();
            printf("threading is done\n");
        });
    }

    struct SerializableBattleAction
    {
        int Action;
        int MoveX, MoveY;
        int ActionX, ActionY;
        int magicID = 0;
        int itemID = -1;
        void print()
        {
            printf("action %d, movex %d, movey %d, actionx %d, actiony %d magic %d, item %d\n", Action, MoveX, MoveY, ActionX, ActionY, magicID, itemID);
        }
    };
    static_assert(sizeof(SerializableBattleAction) == 28, "introduced extra struct padding");

    // 每次行动结束 传输Action出去 并且接收
    bool sendMyAction(const BattleNetwork::SerializableBattleAction& action);

    // async，成功后
    bool getOpponentAction(BattleNetwork::SerializableBattleAction& action, std::function<void(std::error_code err, std::size_t bytes)> f);

    bool isHost();

    void addValidation(std::array<unsigned char, VALSIZE>&& bytes);
    void handshake(std::vector<RoleSave>&& my_roles, std::function<void(std::error_code err)> f);
    virtual void getResults(unsigned int& seed, int& friends, std::vector<RoleSave>& final_roles);

protected:
    void nameSetup();
    virtual void getRandSeed() = 0;
    virtual void waitConnection() = 0;

    // 己方参战id，最终roles结果
    virtual void rDataHandshake() = 0;
    virtual void validate(); 

    asio::io_context io_context_;
    asio::ip::tcp::resolver resolver_;
    asio::ip::tcp::socket socket_;
    asio::io_service::work work_;
    // asio::io_context::strand strand_;

    bool is_host_;

    std::thread worker_;
    std::string strID_;
    std::string port_;
    asio::ip::tcp::resolver::query query_;

    std::vector<asio::const_buffer> const_bufs_;
    std::vector<asio::mutable_buffer> mut_bufs_;
    int int_buf_;
    int int_buf2_;

    std::vector<RoleSave> friends_;
    std::vector<RoleSave> role_result_;
    std::function<void(std::error_code err)> final_callback_;
    unsigned int seed_;

    std::vector<std::array<unsigned char, VALSIZE>> validations_;
    std::vector<std::array<unsigned char, VALSIZE>> op_validations_;
};

class BattleHost : public BattleNetwork
{
public:
    BattleHost(const std::string& strID, const std::string& port);

protected:
    void waitConnection() override;
    void getRandSeed() override;
    void rDataHandshake() override;
};

class BattleClient : public BattleNetwork
{
public:
    static const int GO = 42;
    BattleClient(const std::string& strID, const std::string& port);

protected:
    void waitConnection() override;
    void getRandSeed() override;
    void rDataHandshake() override;

    int go_ = GO;
};

class BattleNetworkFactory
{
public:
    // 等待连接
    static std::unique_ptr<BattleNetwork> MakeHost(const std::string& id);
    static std::unique_ptr<BattleNetwork> MakeClient(const std::string& id);

private:
    static bool UI(BattleNetwork* net);
};
#else
//假的Network
class BattleNetwork
{
public:
    static const std::size_t VALSIZE = 32;
    struct SerializableBattleAction
    {
        int Action;
        int MoveX, MoveY;
        int ActionX, ActionY;
        int magicID = 0;
        int itemID = -1;
        void print() {}
    };
    bool sendMyAction(const BattleNetwork::SerializableBattleAction& action) { return false; }
    bool getOpponentAction(BattleNetwork::SerializableBattleAction& action, std::function<void(std::error_code err, std::size_t bytes)> f) { return false; }
    bool isHost() { return false; }
    void addValidation(std::array<unsigned char, VALSIZE>&& bytes) {}
    void handshake(std::vector<RoleSave>&& my_roles, std::function<void(std::error_code err)> f) {}
    virtual void getResults(unsigned int& seed, int& friends, std::vector<RoleSave>& final_roles) {}
};
class BattleNetworkFactory
{
public:
    // 等待连接
    static std::unique_ptr<BattleNetwork> MakeHost(const std::string& id) { return nullptr; }
    static std::unique_ptr<BattleNetwork> MakeClient(const std::string& id) { return nullptr; }
};
#endif

#pragma once

#include "asio.hpp"
#include "BattleMenu.h"
#include <thread>
#include <memory>


// #define _WIN32_WINNT 0x0A00

class BattleNetwork {
public:
    // , strand_(io_context_) 
    BattleNetwork(const std::string& strID) : socket_(io_context_), work_(io_context_), strID_(strID) {};

    virtual ~BattleNetwork() {
        io_context_.stop();
        if (worker_.joinable()) worker_.join();
    }

    void asyncRun() {
        /*
        printf("asyncRun started\n");
        if (worker_.joinable())
            worker_.join();
        */
        worker_ = std::thread([this] {
            printf("spawning thread\n");
            io_context_.run();
            printf("threading is done\n");
        });
    }

    virtual void waitConnection(int& letItGo, std::function<void(std::error_code err, std::size_t bytes)> f) = 0;
    virtual bool getRandSeed(unsigned int& seed) = 0;

    // 己方参战id，最终roles结果
    virtual void rDataHandshake(std::vector<RoleSave>& my_roles, std::vector<RoleSave>& roles) = 0;

    struct SerializableBattleAction
    {
        int Action;
        int MoveX, MoveY;
        int ActionX, ActionY;
        int magicID = 0;
        int itemID = -1;
        void print() {
            printf("action %d, movex %d, movey %d, actionx %d, actiony %d magic %d, item %d\n", Action, MoveX, MoveY, ActionX, ActionY, magicID, itemID);
        }
    };
    static_assert(sizeof(SerializableBattleAction) == 28, "introduced extra struct padding");

    // 每次行动结束 传输Action出去 并且接收
    bool sendMyAction(const BattleNetwork::SerializableBattleAction& action);

    // async，成功后
    bool getOpponentAction(BattleNetwork::SerializableBattleAction& action, std::function<void(std::error_code err, std::size_t bytes)> f);

    void setup();

    bool hasErr();
    bool isHost();

protected:
    void makeConnection(const std::string& port);

    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
    asio::io_service::work  work_;
    // asio::io_context::strand strand_;
    bool is_host_;
    bool err_ = false;
    std::thread worker_;
    std::string strID_;
};


class BattleHost : public BattleNetwork {
public:
    BattleHost(const std::string& strID);
    void waitConnection(int& letItGo, std::function<void(std::error_code err, std::size_t bytes)> f) override;
    bool getRandSeed(unsigned int& seed) override;
    void rDataHandshake(std::vector<RoleSave>& my_roles, std::vector<RoleSave>& roles) override;
};

class BattleClient : public BattleNetwork {
public:
    static const int GO = 42;
    BattleClient(const std::string& strID);
    void waitConnection(int& letItGo, std::function<void(std::error_code err, std::size_t bytes)> f) override;
    bool getRandSeed(unsigned int& seed) override;
    void rDataHandshake(std::vector<RoleSave>& my_roles, std::vector<RoleSave>& roles) override;
private:
    int go_ = GO;
};

class BattleNetworkFactory {
public:
    // 等待连接
    static std::unique_ptr<BattleNetwork> MakeHost(const std::string& id);
    static std::unique_ptr<BattleNetwork> MakeClient(const std::string& id);
};

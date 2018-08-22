#include "BattleNetwork.h"
#include "GameUtil.h"

#include <random>


using asio::ip::tcp;

bool BattleNetwork::sendMyAction(const BattleNetwork::SerializableBattleAction & action)
{
    // 错误不管
    printf("sendMyAction\n");
    asio::async_write(socket_, asio::buffer(&action, sizeof(action)), [](std::error_code err, std::size_t bytes) {
        printf("send %s\n", err.message().c_str());
    });
    // asio::write(socket_, asio::buffer(&action, sizeof(action)));
    return true;
}

bool BattleNetwork::getOpponentAction(BattleNetwork::SerializableBattleAction & action, std::function<void(std::error_code err, std::size_t bytes)> f)
{
    printf("getOpponentAction\n");
    asio::async_read(socket_, asio::buffer(&action, sizeof(action)), f);
    // asio::read(socket_, asio::buffer(&action, sizeof(action)));
    return true;
}

void BattleNetwork::setup()
{
    if (err_) return;
    int len = strID_.size();
    asio::write(socket_, asio::buffer(&len, sizeof(len)));
    asio::write(socket_, asio::buffer(strID_.data(), len));
    int result;
    asio::read(socket_, asio::buffer(&result, sizeof(result)));
    err_ = result == 0;
}

bool BattleNetwork::hasErr()
{
    return err_;
}

bool BattleNetwork::isHost()
{
    return is_host_;
}

void BattleNetwork::makeConnection(const std::string & port)
{
    tcp::resolver resolver(io_context_);
    // 从ini读取吧，现在先localhost，等我搭建好
    std::string server = GameUtil::getInstance()->getString("network", "server", "138.197.200.52");
    asio::ip::tcp::resolver::query q(server, port);
    asio::error_code ec;
    auto endpoints = resolver.resolve(q, ec);
    printf("resolver %s\n", ec.message().c_str());
    asio::connect(socket_, endpoints, ec);
    printf("connection %s\n", ec.message().c_str());
    if (ec) {
        err_ = true;
    }
}


std::unique_ptr<BattleNetwork> BattleNetworkFactory::MakeHost(const std::string& id)
{
    auto host = std::make_unique<BattleHost>(id);
    host->asyncRun();
    host->setup();
    return std::move(host);
}

std::unique_ptr<BattleNetwork> BattleNetworkFactory::MakeClient(const std::string& id)
{
    auto client = std::make_unique<BattleClient>(id);
    client->asyncRun();
    client->setup();
    return std::move(client);
}

BattleHost::BattleHost(const std::string& strID) : BattleNetwork(strID)
{
    is_host_ = true;
    makeConnection("31111");
}

void BattleHost::waitConnection(std::function<void(std::error_code err, std::size_t bytes)> f)
{
    int letItGo;
    asio::async_read(socket_, asio::buffer(&letItGo, sizeof(letItGo)), f);
}

bool BattleHost::getRandSeed(unsigned int & seed)
{
    std::random_device device;
    seed = device();
    socket_.send(asio::buffer(&seed, sizeof(seed)));
    return true;
}

void BattleHost::rDataHandshake(std::vector<RoleSave>& my_roles, std::vector<RoleSave>& roles)
{
    // 先传输人数
    size_t num = my_roles.size();
    socket_.send(asio::buffer(&num, sizeof(num)));
    for (int i = 0; i < num; i++) {
        socket_.send(asio::buffer(&my_roles[i], sizeof(RoleSave)));
        roles.push_back(my_roles[i]);
    }

    // 获取对面人数
    asio::read(socket_, asio::buffer(&num, sizeof(num)));
    for (int i = 0; i < num; i++) {
        RoleSave r;
        asio::read(socket_, asio::buffer(&r, sizeof(RoleSave)));
        roles.push_back(r);
    }
}

BattleClient::BattleClient(const std::string& strID) : BattleNetwork(strID)
{
    is_host_ = false;
    makeConnection("31112");
}

void BattleClient::waitConnection(std::function<void(std::error_code err, std::size_t bytes)> f)
{
    int letItGo = 42;
    asio::async_write(socket_, asio::buffer(&letItGo, sizeof(letItGo)), f);
}

bool BattleClient::getRandSeed(unsigned int & seed)
{
    asio::read(socket_, asio::buffer(&seed, sizeof(seed)));
    return true;
}

void BattleClient::rDataHandshake(std::vector<RoleSave>& my_roles, std::vector<RoleSave>& roles)
{
    // 获取对面人数
    size_t num;
    asio::read(socket_, asio::buffer(&num, sizeof(num)));
    for (int i = 0; i < num; i++) {
        RoleSave r;
        asio::read(socket_, asio::buffer(&r, sizeof(RoleSave)));
        roles.push_back(r);
    }

    // 传输人数
    num = my_roles.size();
    socket_.send(asio::buffer(&num, sizeof(num)));
    for (int i = 0; i < num; i++) {
        socket_.send(asio::buffer(&my_roles[i], sizeof(RoleSave)));
        roles.push_back(my_roles[i]);
    }
}
#include "BattleNetwork.h"
#include <random>

using asio::ip::tcp;

bool BattleNetwork::sendMyAction(const BattleNetwork::SerializableBattleAction & action)
{
    // 错误不管
    printf("sendMyAction\n");
    asio::async_write(socket_, asio::buffer(&action, sizeof(action)), [](std::error_code err, std::size_t bytes) {
        printf("send %s\n", err.message().c_str());
    });
    return true;
}

bool BattleNetwork::getOpponentAction(BattleNetwork::SerializableBattleAction & action, std::function<void(std::error_code err, std::size_t bytes)> f)
{
    printf("getOpponentAction\n");
    asio::async_read(socket_, asio::buffer(&action, sizeof(action)), f);
    return true;
}

bool BattleNetwork::isHost()
{
    return is_host_;
}


std::unique_ptr<BattleNetwork> BattleNetworkFactory::MakeHost()
{
    auto host = std::make_unique<BattleHost>();
    host->asyncRun();
    return std::move(host);
}

std::unique_ptr<BattleNetwork> BattleNetworkFactory::MakeClient(const std::string & host, const std::string & port)
{
    auto client = std::make_unique<BattleClient>(host, port);
    client->asyncRun();
    return std::move(client);
}

BattleHost::BattleHost()
{
    tcp::acceptor acceptor(io_context_, tcp::endpoint(tcp::v4(), 8122));
    acceptor.accept(socket_);
    is_host_ = true;
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

BattleClient::BattleClient(const std::string& host, const std::string& port)
{
    printf("trying to connect to %s, %s\n", host.c_str(), port.c_str());
    tcp::resolver resolver(io_context_);
    asio::ip::tcp::resolver::query q(host, port);
    asio::error_code ec;
    auto endpoints = resolver.resolve(q, ec);
    printf("resolver %s\n", ec.message().c_str());
    asio::connect(socket_, endpoints, ec);
    printf("connection %s\n", ec.message().c_str());
    is_host_ = false;
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
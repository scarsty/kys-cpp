#include "BattleNetwork.h"
#include <random>

using asio::ip::tcp;

bool BattleNetwork::sendMyAction(const BattleNetwork::SerializableBattleAction & action)
{
    // ´íÎó²»¹Ü
    printf("sendMyAction\n");
    asio::async_write(socket_, asio::buffer(&action, sizeof(action)), [](std::error_code err, std::size_t bytes) {
        printf("send %s\n", err.message());
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
    auto device = std::random_device();
    seed = device();
    socket_.send(asio::buffer(&seed, sizeof(seed)));
    return true;
}

bool BattleHost::getOpponentRoleID(int my_id, int & your_id)
{
    socket_.send(asio::buffer(&my_id, sizeof(my_id)));
    asio::read(socket_, asio::buffer(&your_id, sizeof(your_id)));
    return true;
}

BattleClient::BattleClient(const std::string& host, const std::string& port)
{
    tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host, port);
    asio::connect(socket_, endpoints);
    is_host_ = false;
}

bool BattleClient::getRandSeed(unsigned int & seed)
{
    asio::read(socket_, asio::buffer(&seed, sizeof(seed)));
    return true;
}

bool BattleClient::getOpponentRoleID(int my_id, int & your_id)
{
    asio::read(socket_, asio::buffer(&your_id, sizeof(your_id)));
    socket_.send(asio::buffer(&my_id, sizeof(my_id)));
    return true;
}

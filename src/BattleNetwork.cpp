#include "BattleNetwork.h"
#include "DrawableOnCall.h"
#include "Font.h"
#include "GameUtil.h"
#include "Save.h"
#include "TeamMenu.h"

#include <random>

#define CALLBACK_ON_ERROR(err) \
    if (err)                   \
    {                          \
        final_callback_(err);  \
        return;                \
    }

bool BattleNetwork::sendMyAction(const BattleNetwork::SerializableBattleAction& action)
{
    // 错误不管
    printf("sendMyAction\n");
    asio::async_write(socket_, asio::buffer(&action, sizeof(action)), [](std::error_code err, std::size_t bytes)
    {
        printf("send %s\n", err.message().c_str());
    });
    return true;
}

bool BattleNetwork::getOpponentAction(BattleNetwork::SerializableBattleAction& action, std::function<void(std::error_code err, std::size_t bytes)> f)
{
    printf("getOpponentAction\n");
    asio::async_read(socket_, asio::buffer(&action, sizeof(action)), f);
    return true;
}

void BattleNetwork::nameSetup()
{
    // 传递名字
    int_buf_ = strID_.size();
    const_bufs_.push_back(asio::buffer(&int_buf_, sizeof(int_buf_)));
    const_bufs_.push_back(asio::buffer(strID_.data(), int_buf_));
    asio::async_write(socket_, const_bufs_, [this](std::error_code err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        asio::async_read(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](std::error_code err, std::size_t bytes)
        {
            // 读取结果，0失败
            CALLBACK_ON_ERROR(err);
            if (int_buf_ == 0)
            {
                err = std::make_error_code(std::errc::not_connected);
                CALLBACK_ON_ERROR(err);
            }
            // 下一步， host等待GO，client发送GO
            int_buf_ = 1;
            printf("waiting loop\n");
            waitConnection();
        });
    });
}

bool BattleNetwork::isHost()
{
    return is_host_;
}

void BattleNetwork::handshake(std::vector<RoleSave>&& my_roles, std::vector<MagicSave>&& magic, std::function<void(std::error_code err)> f)
{
    final_callback_ = f;
    friends_ = std::move(my_roles);
    magics_ = std::move(magic);
    resolver_.async_resolve(query_, [this](std::error_code err, asio::ip::tcp::resolver::iterator iter)
    {
        CALLBACK_ON_ERROR(err);
        asio::async_connect(socket_, iter, [this](const std::error_code err, asio::ip::tcp::resolver::iterator iter)
        {
            CALLBACK_ON_ERROR(err);
            nameSetup();
        });
    });
}

void BattleNetwork::getResults(unsigned int& seed, int& friends, std::vector<RoleSave>& final_roles)
{
    seed = seed_;
    friends = friends_.size();
    final_roles = std::move(role_result_);
}

BattleHost::BattleHost(const std::string& strID, const std::string& port)
    : BattleNetwork(strID, port)
{
    is_host_ = true;
}

void BattleHost::waitConnection()
{
    asio::async_read(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        printf("got %d\n", int_buf_);
        if (int_buf_ == BattleClient::GO)
        {
            getRandSeed();
        }
        else
        {
            waitConnection();
        }
    });
}

void BattleHost::getRandSeed()
{
    printf("exchange protocol started\n");
    std::random_device device;
    seed_ = device();
    asio::async_write(socket_, asio::buffer(&seed_, sizeof(seed_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        rDataHandshake();
    });
}

void BattleHost::rDataHandshake()
{
    // 先传输人数
    printf("rDataHandshake\n");
    int_buf_ = friends_.size();
    asio::async_write(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        const_bufs_.clear();
        for (int i = 0; i < int_buf_; i++)
        {
            const_bufs_.push_back(asio::buffer(&friends_[i], sizeof(RoleSave)));
            role_result_.push_back(friends_[i]);
        }
        asio::async_write(socket_, const_bufs_, [this](const std::error_code& err, std::size_t bytes)
        {
            CALLBACK_ON_ERROR(err);
            // 获取对面人数
            asio::async_read(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
            {
                CALLBACK_ON_ERROR(err);
                mut_bufs_.clear();
                role_result_.reserve(friends_.size() + int_buf_);
                for (int i = 0; i < int_buf_; i++)
                {
                    role_result_.emplace_back();
                    mut_bufs_.push_back(asio::buffer(&role_result_.back(), sizeof(RoleSave)));
                }
                asio::async_read(socket_, mut_bufs_, [this](const std::error_code& err, std::size_t bytes)
                {
                    CALLBACK_ON_ERROR(err);
                    // 数据收集完毕
                    concileMagicData();
                });
            });
        });
    });
}

void BattleHost::concileMagicData()
{
    // 先送，再读
    printf("concileMagicData\n");
    int_buf_ = magics_.size();
    printf("host magic length %d\n", int_buf_);
    asio::async_write(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        const_bufs2_.clear();
        for (int i = 0; i < magics_.size(); i++)
        {
            const_bufs2_.push_back(asio::buffer(&magics_[i], sizeof(MagicSave)));
        }
        asio::async_write(socket_, const_bufs2_, [this](const std::error_code& err, std::size_t bytes)
        {
            CALLBACK_ON_ERROR(err);
            printf("sent my magic %d\n", bytes);
            // 获取对面的武功
            asio::async_read(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
            {
                CALLBACK_ON_ERROR(err);
                printf("read magic length %d\n", int_buf_);
                opponent_magics_.resize(int_buf_);
                mut_bufs2_.clear();
                for (int i = 0; i < int_buf_; i++)
                {
                    mut_bufs2_.push_back(asio::buffer(&opponent_magics_[i], sizeof(MagicSave)));
                }
                asio::async_read(socket_, mut_bufs2_, [this](const std::error_code& err, std::size_t bytes)
                {
                    CALLBACK_ON_ERROR(err);
                    printf("all done %d\n", bytes);
                    // 数据收集完毕
                    bool match = magics_.size() == opponent_magics_.size();
                    match = match && 0 == std::memcmp(&magics_[0], &opponent_magics_[0], magics_.size() * sizeof(MagicSave));
                    if (!match)
                    {
                        // 随便选一个
                        std::error_code err = std::make_error_code(std::errc::protocol_error);
                        CALLBACK_ON_ERROR(err);
                    }
                    else
                    {
                        // 全部搞完了，可以开战
                        final_callback_(err);
                    }
                });
            });
        });
    });
}

BattleClient::BattleClient(const std::string& strID, const std::string& port)
    : BattleNetwork(strID, port)
{
    is_host_ = false;
}

void BattleClient::waitConnection()
{
    int_buf_ = BattleClient::GO;
    asio::async_write(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        getRandSeed();
    });
}

void BattleClient::getRandSeed()
{
    printf("exchange protocol started\n");
    asio::async_read(socket_, asio::buffer(&seed_, sizeof(seed_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        rDataHandshake();
    });
}

void BattleClient::rDataHandshake()
{
    printf("rDataHandshake\n");
    // 读取人数
    asio::async_read(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        mut_bufs_.clear();
        role_result_.reserve(friends_.size() + int_buf_);
        // 对面
        for (int i = 0; i < int_buf_; i++)
        {
            role_result_.emplace_back();
            mut_bufs_.push_back(asio::buffer(&role_result_.back(), sizeof(RoleSave)));
        }
        asio::async_read(socket_, mut_bufs_, [this](const std::error_code& err, std::size_t bytes)
        {
            CALLBACK_ON_ERROR(err);
            int_buf_ = friends_.size();
            asio::async_write(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
            {
                CALLBACK_ON_ERROR(err);
                // 送人
                const_bufs_.clear();
                for (int i = 0; i < int_buf_; i++)
                {
                    const_bufs_.push_back(asio::buffer(&friends_[i], sizeof(RoleSave)));
                    role_result_.push_back(friends_[i]);
                }
                asio::async_write(socket_, const_bufs_, [this](const std::error_code& err, std::size_t bytes)
                {
                    CALLBACK_ON_ERROR(err);
                    concileMagicData();
                });
            });
        });
    });
}

void BattleClient::concileMagicData()
{
    // 先读对面的
    printf("concileMagicData\n");
    asio::async_read(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        printf("client read magic length %d\n", int_buf_);
        mut_bufs2_.clear();
        opponent_magics_.resize(int_buf_);
        // 对面
        for (int i = 0; i < opponent_magics_.size(); i++)
        {
            mut_bufs2_.push_back(asio::buffer(&opponent_magics_[i], sizeof(MagicSave)));
        }
        asio::async_read(socket_, mut_bufs2_, [this](const std::error_code& err, std::size_t bytes)
        {
            CALLBACK_ON_ERROR(err);
            printf("got magic %d\n", bytes);
            int_buf_ = magics_.size();
            asio::async_write(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
            {
                CALLBACK_ON_ERROR(err);
                // 送人
                const_bufs2_.clear();
                for (int i = 0; i < int_buf_; i++)
                {
                    const_bufs2_.push_back(asio::buffer(&magics_[i], sizeof(MagicSave)));
                }
                asio::async_write(socket_, const_bufs2_, [this](const std::error_code& err, std::size_t bytes)
                {
                    CALLBACK_ON_ERROR(err);
                    printf("all done\n");
                    // 数据收集完毕
                    bool match = magics_.size() == opponent_magics_.size();
                    match = match && 0 == std::memcmp(&magics_[0], &opponent_magics_[0], magics_.size() * sizeof(MagicSave));
                    if (!match)
                    {
                        // 随便选一个
                        std::error_code err = std::make_error_code(std::errc::protocol_error);
                        CALLBACK_ON_ERROR(err);
                    }
                    else
                    {
                        // 全部搞完了，可以开战
                        final_callback_(err);
                    }
                });
            });
        });
    });
}

std::unique_ptr<BattleNetwork> BattleNetworkFactory::MakeHost(const std::string& id)
{
    auto host = std::make_unique<BattleHost>(id, "31111");
    host->asyncRun();
    if (!UI(host.get()))
    {
        return nullptr;
    }
    return std::move(host);
}

std::unique_ptr<BattleNetwork> BattleNetworkFactory::MakeClient(const std::string& id)
{
    auto client = std::make_unique<BattleClient>(id, "31112");
    client->asyncRun();
    if (!UI(client.get()))
    {
        return nullptr;
    }
    return std::move(client);
}

bool BattleNetworkFactory::UI(BattleNetwork* net)
{
    // 选择队友
    TeamMenu team;
    team.setMode(1);
    team.run();
    auto friends = team.getRoles();

    std::vector<RoleSave> serializableRoles;
    for (auto r : friends)
    {
        RoleSave me;
        std::memcpy(&me, r, sizeof(me));
        serializableRoles.push_back(me);
    }

    std::vector<MagicSave> serializableMagics;
    for (auto magic : Save::getInstance()->getMagics())
    {
        MagicSave ms;
        std::memcpy(&ms, magic, sizeof(ms));
        serializableMagics.push_back(ms);
    }

    auto f = [](DrawableOnCall* d)
    {
        Font::getInstance()->draw("等待对方玩家连接...", 40, 30, 30, { 200, 200, 50, 255 });
    };

    bool ok = false;
    DrawableOnCall waitThis(f);
    auto exit = [&waitThis, &ok](std::error_code err)
    {
        printf("recv %s\n", err.message().c_str());
        ok = !err;
        waitThis.setExit(true);
    };
    waitThis.setEntrance([&net, &serializableRoles, &serializableMagics, exit]()
    {
        net->handshake(std::move(serializableRoles), std::move(serializableMagics), exit);
    });

    waitThis.run();
    return ok;
}

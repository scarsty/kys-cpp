#include "BattleNetwork.h"

#ifdef WITH_NETWORK
#include "DrawableOnCall.h"
#include "filefunc.h"
#include "Font.h"
#include "GameUtil.h"
#include "Save.h"
#include "TeamMenu.h"
#include "picosha2.h"

#include <algorithm>
#include <cstring>
#include <random>

#define CALLBACK_ON_ERROR(err) if (err) { final_callback_(err); return; }

namespace
{
BattleNetwork::SerializableRole serializeRole(const RoleSave& role)
{
    BattleNetwork::SerializableRole result{};
    result.ID = role.ID;
    result.HeadID = role.HeadID;
    result.IncLife = role.IncLife;
    result.UnUse = role.UnUse;
    std::copy_n(role.Name.data(), std::min(role.Name.size(), result.Name.size() - 1), result.Name.begin());
    std::copy_n(role.Nick.data(), std::min(role.Nick.size(), result.Nick.size() - 1), result.Nick.begin());
    result.Sexual = role.Sexual;
    result.Level = role.Level;
    result.Exp = role.Exp;
    result.HP = role.HP;
    result.MaxHP = role.MaxHP;
    result.Hurt = role.Hurt;
    result.Poison = role.Poison;
    result.PhysicalPower = role.PhysicalPower;
    result.ExpForMakeItem = role.ExpForMakeItem;
    result.Equip0 = role.Equip0;
    result.Equip1 = role.Equip1;
    std::copy(std::begin(role.EquipMagic), std::end(role.EquipMagic), std::begin(result.EquipMagic));
    std::copy(std::begin(role.EquipMagic2), std::end(role.EquipMagic2), std::begin(result.EquipMagic2));
    result.EquipItem = role.EquipItem;
    std::copy(std::begin(role.Frame), std::end(role.Frame), std::begin(result.Frame));
    result.MPType = role.MPType;
    result.MP = role.MP;
    result.MaxMP = role.MaxMP;
    result.Attack = role.Attack;
    result.Speed = role.Speed;
    result.Defence = role.Defence;
    result.Medicine = role.Medicine;
    result.UsePoison = role.UsePoison;
    result.Detoxification = role.Detoxification;
    result.AntiPoison = role.AntiPoison;
    result.Fist = role.Fist;
    result.Sword = role.Sword;
    result.Knife = role.Knife;
    result.Unusual = role.Unusual;
    result.HiddenWeapon = role.HiddenWeapon;
    result.Knowledge = role.Knowledge;
    result.Morality = role.Morality;
    result.AttackWithPoison = role.AttackWithPoison;
    result.AttackTwice = role.AttackTwice;
    result.Fame = role.Fame;
    result.IQ = role.IQ;
    result.PracticeItem = role.PracticeItem;
    result.ExpForItem = role.ExpForItem;
    std::copy(std::begin(role.MagicID), std::end(role.MagicID), std::begin(result.MagicID));
    std::copy(std::begin(role.MagicLevel), std::end(role.MagicLevel), std::begin(result.MagicLevel));
    std::copy(std::begin(role.TakingItem), std::end(role.TakingItem), std::begin(result.TakingItem));
    std::copy(std::begin(role.TakingItemCount), std::end(role.TakingItemCount), std::begin(result.TakingItemCount));
    std::copy(std::begin(role.InternalID), std::end(role.InternalID), std::begin(result.InternalID));
    std::copy(std::begin(role.InternalLevel), std::end(role.InternalLevel), std::begin(result.InternalLevel));
    return result;
}

RoleSave deserializeRole(const BattleNetwork::SerializableRole& role)
{
    RoleSave result{};
    result.ID = role.ID;
    result.HeadID = role.HeadID;
    result.IncLife = role.IncLife;
    result.UnUse = role.UnUse;
    result.Name = role.Name.data();
    result.Nick = role.Nick.data();
    result.Sexual = role.Sexual;
    result.Level = role.Level;
    result.Exp = role.Exp;
    result.HP = role.HP;
    result.MaxHP = role.MaxHP;
    result.Hurt = role.Hurt;
    result.Poison = role.Poison;
    result.PhysicalPower = role.PhysicalPower;
    result.ExpForMakeItem = role.ExpForMakeItem;
    result.Equip0 = role.Equip0;
    result.Equip1 = role.Equip1;
    std::copy(std::begin(role.EquipMagic), std::end(role.EquipMagic), std::begin(result.EquipMagic));
    std::copy(std::begin(role.EquipMagic2), std::end(role.EquipMagic2), std::begin(result.EquipMagic2));
    result.EquipItem = role.EquipItem;
    std::copy(std::begin(role.Frame), std::end(role.Frame), std::begin(result.Frame));
    result.MPType = role.MPType;
    result.MP = role.MP;
    result.MaxMP = role.MaxMP;
    result.Attack = role.Attack;
    result.Speed = role.Speed;
    result.Defence = role.Defence;
    result.Medicine = role.Medicine;
    result.UsePoison = role.UsePoison;
    result.Detoxification = role.Detoxification;
    result.AntiPoison = role.AntiPoison;
    result.Fist = role.Fist;
    result.Sword = role.Sword;
    result.Knife = role.Knife;
    result.Unusual = role.Unusual;
    result.HiddenWeapon = role.HiddenWeapon;
    result.Knowledge = role.Knowledge;
    result.Morality = role.Morality;
    result.AttackWithPoison = role.AttackWithPoison;
    result.AttackTwice = role.AttackTwice;
    result.Fame = role.Fame;
    result.IQ = role.IQ;
    result.PracticeItem = role.PracticeItem;
    result.ExpForItem = role.ExpForItem;
    std::copy(std::begin(role.MagicID), std::end(role.MagicID), std::begin(result.MagicID));
    std::copy(std::begin(role.MagicLevel), std::end(role.MagicLevel), std::begin(result.MagicLevel));
    std::copy(std::begin(role.TakingItem), std::end(role.TakingItem), std::begin(result.TakingItem));
    std::copy(std::begin(role.TakingItemCount), std::end(role.TakingItemCount), std::begin(result.TakingItemCount));
    std::copy(std::begin(role.InternalID), std::end(role.InternalID), std::begin(result.InternalID));
    std::copy(std::begin(role.InternalLevel), std::end(role.InternalLevel), std::begin(result.InternalLevel));
    return result;
}
}

bool BattleNetwork::sendMyAction(const BattleNetwork::SerializableBattleAction& action)
{
    // 错误不管
    LOG("sendMyAction\n");
    asio::async_write(socket_, asio::buffer(&action, sizeof(action)), [](std::error_code err, std::size_t bytes)
    {
        LOG("send {}\n", err.message());
    });
    return true;
}

bool BattleNetwork::getOpponentAction(BattleNetwork::SerializableBattleAction& action, std::function<void(std::error_code err, std::size_t bytes)> f)
{
    LOG("getOpponentAction\n");
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
            LOG("waiting loop\n");
            waitConnection();
        });
    });
}

bool BattleNetwork::isHost()
{
    return is_host_;
}

void BattleNetwork::addValidation(std::array<unsigned char, 32>&& bytes)
{
    validations_.push_back(std::move(bytes));
}

void BattleNetwork::handshake(std::vector<RoleSave>&& my_roles, std::function<void(std::error_code err)> f)
{
    final_callback_ = f;
    friends_.clear();
    friends_.reserve(my_roles.size());
    for (const auto& role : my_roles)
    {
        friends_.push_back(serializeRole(role));
    }
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
    final_roles.clear();
    final_roles.reserve(role_result_.size());
    for (const auto& role : role_result_)
    {
        final_roles.push_back(deserializeRole(role));
    }
}

void BattleNetwork::validate()
{
    int_buf_ = validations_.size();
    asio::async_write(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        asio::async_read(socket_, asio::buffer(&int_buf2_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
        {
            CALLBACK_ON_ERROR(err);
            if (int_buf_ != int_buf2_)
            {
                std::error_code err = std::make_error_code(std::errc::protocol_error);
                CALLBACK_ON_ERROR(err);
            }
            const_bufs_.clear();
            for (auto& v : validations_)
            {
                const_bufs_.push_back(asio::buffer(v));
            }
            asio::async_write(socket_, const_bufs_, [this](const std::error_code& err, std::size_t bytes)
            {
                CALLBACK_ON_ERROR(err);
                mut_bufs_.clear();
                op_validations_.resize(validations_.size());
                for (int i = 0; i < int_buf_; i++)
                {
                    mut_bufs_.push_back(asio::buffer(op_validations_[i]));
                }
                asio::async_read(socket_, mut_bufs_, [this](const std::error_code& err, std::size_t bytes)
                {
                    CALLBACK_ON_ERROR(err);
                    for (int i = 0; i < validations_.size(); i++)
                    {
                        if (validations_[i] != op_validations_[i])
                        {
                            std::error_code err = std::make_error_code(std::errc::protocol_error);
                            CALLBACK_ON_ERROR(err);
                        }
                    }
                    // ok
                    final_callback_(err);
                });
            });
        });
    });
}

BattleHost::BattleHost(const std::string& strID, const std::string& port) : BattleNetwork(strID, port)
{
    is_host_ = true;
}

void BattleHost::waitConnection()
{
    asio::async_read(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        LOG("got {}\n", int_buf_);
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
    LOG("exchange protocol started\n");
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
    LOG("rDataHandshake\n");
    int_buf_ = friends_.size();
    asio::async_write(socket_, asio::buffer(&int_buf_, sizeof(int_buf_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        const_bufs_.clear();
        for (int i = 0; i < int_buf_; i++)
        {
            const_bufs_.push_back(asio::buffer(&friends_[i], sizeof(SerializableRole)));
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
                    mut_bufs_.push_back(asio::buffer(&role_result_.back(), sizeof(SerializableRole)));
                }
                asio::async_read(socket_, mut_bufs_, [this](const std::error_code& err, std::size_t bytes)
                {
                    CALLBACK_ON_ERROR(err);
                    // 数据收集完毕，开始验证（或许我应该先验证，不管了）
                    validate();
                });
            });
        });
    });
}

BattleClient::BattleClient(const std::string& strID, const std::string& port) : BattleNetwork(strID, port)
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
    LOG("exchange protocol started\n");
    asio::async_read(socket_, asio::buffer(&seed_, sizeof(seed_)), [this](const std::error_code& err, std::size_t bytes)
    {
        CALLBACK_ON_ERROR(err);
        rDataHandshake();
    });
}

void BattleClient::rDataHandshake()
{
    LOG("rDataHandshake\n");
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
            mut_bufs_.push_back(asio::buffer(&role_result_.back(), sizeof(SerializableRole)));
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
                    const_bufs_.push_back(asio::buffer(&friends_[i], sizeof(SerializableRole)));
                    role_result_.push_back(friends_[i]);
                }
                asio::async_write(socket_, const_bufs_, [this](const std::error_code& err, std::size_t bytes)
                {
                    CALLBACK_ON_ERROR(err);
                    validate();
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
    auto team = std::make_shared<TeamMenu>();
    team->setMode(1);
    team->run();
    auto friends = team->getRoles();

    std::vector<RoleSave> serializableRoles;
    for (auto r : friends)
    {
        serializableRoles.push_back(static_cast<const RoleSave&>(*r));
    }

    static_assert(BattleNetwork::VALSIZE == picosha2::k_digest_size, "validation size mismatch");
    // 版本验证
    std::array<unsigned char, BattleNetwork::VALSIZE> version = { 0 };
    std::string verStr(GameUtil::VERSION());
    if (verStr.size() > BattleNetwork::VALSIZE)
    {
        return false;
    }
    std::memcpy(&version[0], &verStr[0], verStr.size());
    net->addValidation(std::move(version));

    const auto MagicSize = sizeof(MagicSave);
    std::vector<unsigned char> magicByteArr(Save::getInstance()->getMagics().size() * MagicSize);
    int idx = 0;
    for (auto magic : Save::getInstance()->getMagics())
    {
        std::memcpy(&magicByteArr[idx * MagicSize], magic, MagicSize);
        idx += 1;
    }
    std::array<unsigned char, BattleNetwork::VALSIZE> magicHash;
    picosha2::hash256(magicByteArr, magicHash);
    net->addValidation(std::move(magicHash));

    auto f = [](DrawableOnCall* d)
    {
        Font::getInstance()->draw("等待对方玩家连接...", 40, 30, 30, { 200, 200, 50, 255 });
    };

    bool ok = false;
    auto waitThis = std::make_shared<DrawableOnCall>(f);
    auto exit = [&waitThis, &ok](std::error_code err)
    {
        LOG("recv {}\n", err.message());
        ok = !err;
        waitThis->setExit(true);
    };
    waitThis->setEntrance([&net, &serializableRoles, exit]()
    {
        net->handshake(std::move(serializableRoles), exit);
    });

    waitThis->run();
    return ok;
}
#endif

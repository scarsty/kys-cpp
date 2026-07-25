#pragma once

#include <cassert>
#include <compare>
#include <cstdint>
#include <limits>

namespace KysChess::Battle
{

class BattleFixed
{
public:
    static constexpr std::int64_t Scale = 1'000'000;

    constexpr BattleFixed() = default;
    constexpr BattleFixed(int value)
        : raw_(static_cast<std::int64_t>(value) * Scale)
    {
    }

    static constexpr BattleFixed fromRaw(std::int64_t raw)
    {
        BattleFixed result;
        result.raw_ = raw;
        return result;
    }

    static constexpr BattleFixed fromInteger(int value)
    {
        return fromRaw(static_cast<std::int64_t>(value) * Scale);
    }

    static constexpr BattleFixed fromRatio(int numerator, int denominator)
    {
        assert(denominator > 0);
        return fromRaw(static_cast<std::int64_t>(numerator) * Scale / denominator);
    }

    constexpr std::int64_t raw() const { return raw_; }
    constexpr int toInt() const { return static_cast<int>(raw_ / Scale); }
    constexpr double toDouble() const { return static_cast<double>(raw_) / Scale; }

    constexpr BattleFixed scaled(std::int64_t numerator, std::int64_t denominator) const
    {
        assert(numerator >= 0);
        assert(denominator > 0);
        if (numerator > 0)
        {
            if (raw_ > 0)
            {
                assert(raw_ <= std::numeric_limits<std::int64_t>::max() / numerator);
            }
            else if (raw_ < 0)
            {
                assert(raw_ >= std::numeric_limits<std::int64_t>::min() / numerator);
            }
        }
        return fromRaw(raw_ * numerator / denominator);
    }

    constexpr BattleFixed multipliedBy(BattleFixed other) const
    {
        if (other.raw_ != 0)
        {
            if (raw_ > 0 && other.raw_ > 0)
            {
                assert(raw_ <= std::numeric_limits<std::int64_t>::max() / other.raw_);
            }
            else if (raw_ < 0 && other.raw_ < 0)
            {
                assert(raw_ >= std::numeric_limits<std::int64_t>::max() / other.raw_);
            }
            else if (raw_ > 0 && other.raw_ < 0)
            {
                assert(other.raw_ >= std::numeric_limits<std::int64_t>::min() / raw_);
            }
            else if (raw_ < 0 && other.raw_ > 0)
            {
                assert(raw_ >= std::numeric_limits<std::int64_t>::min() / other.raw_);
            }
        }
        return fromRaw(raw_ * other.raw_ / Scale);
    }

    constexpr BattleFixed dividedBy(BattleFixed other) const
    {
        assert(other.raw_ != 0);
        if (raw_ != 0)
        {
            if (raw_ > 0)
            {
                assert(raw_ <= std::numeric_limits<std::int64_t>::max() / Scale);
            }
            else
            {
                assert(raw_ >= std::numeric_limits<std::int64_t>::min() / Scale);
            }
        }
        return fromRaw(raw_ * Scale / other.raw_);
    }

    constexpr BattleFixed& operator+=(BattleFixed other)
    {
        raw_ += other.raw_;
        return *this;
    }

    constexpr BattleFixed& operator-=(BattleFixed other)
    {
        raw_ -= other.raw_;
        return *this;
    }

    friend constexpr BattleFixed operator+(BattleFixed lhs, BattleFixed rhs)
    {
        lhs += rhs;
        return lhs;
    }

    friend constexpr BattleFixed operator-(BattleFixed lhs, BattleFixed rhs)
    {
        lhs -= rhs;
        return lhs;
    }

    auto operator<=>(const BattleFixed&) const = default;

private:
    std::int64_t raw_{};
};

}

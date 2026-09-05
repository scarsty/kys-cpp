#pragma once

#include <algorithm>
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
    constexpr int toInt() const
    {
        return static_cast<int>(std::clamp<std::int64_t>(
            raw_ / Scale,
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max()));
    }
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

    constexpr BattleFixed scaledPercentSaturated(std::int64_t factor) const
    {
        assert(factor >= 0);
        constexpr std::int64_t denominator = 100;
        constexpr std::int64_t maximumRaw =
            static_cast<std::int64_t>(std::numeric_limits<int>::max()) * Scale;
        constexpr std::int64_t minimumRaw =
            static_cast<std::int64_t>(std::numeric_limits<int>::min()) * Scale;
        const std::int64_t quotient = raw_ / denominator;
        const std::int64_t remainder = raw_ % denominator;
        if (factor > 0)
        {
            if (quotient > 0 && quotient > maximumRaw / factor)
                return fromRaw(maximumRaw);
            if (quotient < 0 && quotient < minimumRaw / factor)
                return fromRaw(minimumRaw);
        }
        const std::int64_t whole = quotient * factor;
        // |remainder| < 100 and percentage factors are formed from an int
        // delta plus 100, so this product cannot overflow int64.
        const std::int64_t fraction = remainder * factor / denominator;
        if (fraction > 0 && whole > maximumRaw - fraction)
            return fromRaw(maximumRaw);
        if (fraction < 0 && whole < minimumRaw - fraction)
            return fromRaw(minimumRaw);
        return fromRaw(std::clamp(whole + fraction, minimumRaw, maximumRaw));
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

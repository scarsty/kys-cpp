#pragma once
#include <random>

enum RandomType
{
    RANDOM_UNIFORM = 0,
    RANDOM_NORMAL = 1,
};

template <typename T = float>
class Random
{
private:
    RandomType type_ = RANDOM_UNIFORM;
    std::random_device device_;
    std::mt19937 generator_;
    std::uniform_real_distribution<T> uniform_dist_{ 0, 1 };
    std::normal_distribution<T> normal_dist_{ 0, 1 };
    std::minstd_rand0 generator_fast_;

public:
    Random() { set_seed(); }

    virtual ~Random() {}

    void set_random_type(RandomType t) { type_ = t; }

    void set_parameter(T a, T b)
    {
        uniform_dist_.param(decltype(uniform_dist_.param())(a, b));
        normal_dist_.param(decltype(normal_dist_.param())(a, b));
    }

    T rand()
    {
        if (type_ == RANDOM_UNIFORM)
        {
            return uniform_dist_(generator_);
        }
        else if (type_ == RANDOM_NORMAL)
        {
            return normal_dist_(generator_);
        }
        return 0;
    }

    T rand_fast()
    {
        if (type_ == RANDOM_UNIFORM)
        {
            return uniform_dist_(generator_fast_);
        }
        else if (type_ == RANDOM_NORMAL)
        {
            return normal_dist_(generator_fast_);
        }
        return 0;
    }

    void set_seed()
    {
        generator_ = std::mt19937(device_());
    }

    void set_seed(unsigned int seed)
    {
        generator_ = std::mt19937(seed);
    }

    int rand_int(int n)
    {
        if (n <= 0)
        {
            return 0;
        }
        return int(rand() * n);
    }
};

typedef Random<double> RandomDouble;    //use this in usual
typedef Random<float> RandomFloat;      //use this in usual

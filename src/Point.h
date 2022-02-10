#pragma once
#include <cmath>

enum Towards
{
    Towards_RightUp = 0,
    Towards_RightDown = 1,
    Towards_LeftUp = 2,
    Towards_LeftDown = 3,
    Towards_None
};

template <typename T>
struct Point_
{
public:
    Point_() {}
    Point_(T x1, T y1, T z1 = 0) : x(x1), y(y1), z(z1) {}
    T x{ 0 }, y{ 0 }, z{ 0 };
    Point_<T>& norm(T n)
    {
        T n1 = sqrt(x * x + y * y + z * z);
        if (n1 != 0)
        {
            x *= n / n1;
            y *= n / n1;
            z *= n / n1;
        }
        return *this;
    }
    Point_<T>& operator*=(double f)
    {
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }
    Point_<T>& operator+=(const Point_<T>& p)
    {
        x += p.x;
        y += p.y;
        z += p.z;
        return *this;
    }
    double getAngle()
    {
        return atan2f(y, x);
    }
};

template <typename T>
inline Point_<T> operator+(const Point_<T> l, const Point_<T> r)
{
    return Point_<T>(l.x + r.x, l.y + r.y, l.z + r.z);
}

template <typename T>
inline Point_<T> operator-(const Point_<T> l, const Point_<T> r)
{
    return Point_<T>(l.x - r.x, l.y - r.y, l.z - r.z);
}

template <typename T>
inline Point_<T> operator-(const Point_<T> l)
{
    return Point_<T>(-l.x, -l.y, -l.z);
}

template <typename T>
inline Point_<T> operator*(double f, const Point_<T> p)
{
    return Point_<T>(p.x * f, p.y * f, p.z * f);
}

template <typename T>
inline Point_<T> operator*(const Point_<T> p, double f)
{
    return Point_<T>(p.x * f, p.y * f, p.z * f);
}

using Point = Point_<int>;
using Pointf = Point_<double>;

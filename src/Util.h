#pragma once
#include <vector>
#include <initializer_list>

class Util
{
public:
    Util();
    ~Util();

    template <class T> static void safe_delete(T*& pointer)
    {
        if (pointer) { delete pointer; }
        pointer = nullptr;
    }
    template <class T> static void safe_delete(std::vector<T*>& pointer_v)
    {
        for (auto& pointer : pointer_v)
        {
            safe_delete(pointer);
        }
    }
    template <class T> static void safe_delete(std::initializer_list<T**> pointer_v)
    {
        for (auto& pointer : pointer_v)
        {
            safe_delete(*pointer);
        }
    }

};


#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <type_traits>

namespace KysChess::Battle
{

template<typename Range, typename Value, typename Projection>
auto tryFindBy(Range& range, const Value& value, Projection projection)
{
    using Reference = std::ranges::range_reference_t<Range>;
    using Element = std::remove_reference_t<Reference>;

    auto it = std::ranges::find(range, value, projection);
    return it == std::ranges::end(range)
        ? static_cast<Element*>(nullptr)
        : std::addressof(*it);
}

template<typename Range>
auto tryFindById(Range& range, int id)
{
    using Element = std::remove_cvref_t<std::ranges::range_reference_t<Range>>;
    return tryFindBy(range, id, &Element::id);
}

template<typename Range, typename Value, typename Projection>
decltype(auto) requireBy(Range& range, const Value& value, Projection projection)
{
    auto* item = tryFindBy(range, value, projection);
    assert(item);
    return *item;
}

template<typename Range>
decltype(auto) requireById(Range& range, int id)
{
    auto* item = tryFindById(range, id);
    assert(item);
    return *item;
}

template<typename Map>
decltype(auto) requireMappedById(Map& map, int id)
{
    auto it = map.find(id);
    assert(it != map.end());
    return (it->second);
}

}  // namespace KysChess::Battle

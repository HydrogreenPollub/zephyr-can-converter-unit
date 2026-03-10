#ifndef INDEXED_TYPE_MAP_HPP
#define INDEXED_TYPE_MAP_HPP

#include <type_traits>
#include <tuple>
#include <cstdint>

template <std::size_t I, typename T>
struct IndexToType{
    using Index = std::integral_constant<std::size_t, I>;
    using Type = T;
};

template <typename T, std::size_t I>
struct TypeToIndex{
    using Index = std::integral_constant<std::size_t, I>;
    using Type = T;
};

template <typename Tuple, std::size_t... Is>
struct IndexedTypeMapImpl:
    IndexToType<Is, std::tuple_element_t<Is, Tuple>>... ,
    TypeToIndex<std::tuple_element_t<Is, Tuple>, Is>...
{
};

namespace detail{

    template <typename Tuple, std::size_t... Is>
    constexpr auto implMakeIndexedTypeMap(Tuple const&, std::index_sequence<Is...>) -> IndexedTypeMapImpl<Tuple, Is...>;

    template <typename... Ts>
    constexpr auto makeIndexedTypeMap() -> decltype(detail::implMakeIndexedTypeMap(std::tuple<Ts...>{}, std::make_index_sequence<sizeof...(Ts)>{}));

} // namespace detail




template <std::size_t I, typename T>
constexpr auto getTypeAtIndex(IndexToType<I, T> const&) -> T;

template <typename T, std::size_t I>
constexpr auto getIndexOfType(TypeToIndex<T, I> const&) { return I;}



template<typename... Ts>
using IndexedTypeMap = decltype(detail::makeIndexedTypeMap<Ts...>());

template <typename Map, std::size_t I>
using TypeAtIndex = decltype(getTypeAtIndex<I>(Map{}));

template <typename Map, typename T>
constexpr std::size_t IndexOfType = getIndexOfType<T>(Map{});






using FundamentalTypes = IndexedTypeMap<
    bool,
    char, signed char, unsigned char,
    wchar_t, char16_t, char32_t,
    short int, unsigned short int,
    int, unsigned int,
    long int, unsigned long int,
    long long int, unsigned long long int,
    float, double, long double
>;


static_assert(std::is_same_v<TypeAtIndex<FundamentalTypes, 0>, bool>);
static_assert(std::is_same_v<TypeAtIndex<FundamentalTypes, 1>, char>);
static_assert(std::is_same_v<TypeAtIndex<FundamentalTypes, 17>, long double>);

static_assert(IndexOfType<FundamentalTypes, bool> == 0);
static_assert(IndexOfType<FundamentalTypes, char> == 1);
static_assert(IndexOfType<FundamentalTypes, long double> == 17);


#endif
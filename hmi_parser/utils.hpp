#ifndef UTILS_HPP
#define UTILS_HPP

#include <type_traits>
#include <cstddef>

template <std::size_t I>
using Index = std::integral_constant<std::size_t, I>;

template <typename U, typename V>
using is_same_removed_cvref_t = typename std::enable_if<std::is_same<
                                            std::remove_cv_t<std::remove_reference_t<U>>,
                                            std::remove_cv_t<std::remove_reference_t<V>>
                                        >::value>::type;

template <typename T, typename Source>
struct mirror_cv
{
    using const_qualified_like_source   = std::conditional_t<std::is_const<Source>::value, std::add_const_t<T>, std::remove_const_t<T>>;
    using cv_qualified_like_source      = std::conditional_t<std::is_volatile<Source>::value, std::add_volatile_t<const_qualified_like_source>, std::remove_volatile_t<const_qualified_like_source>>;

    using type = cv_qualified_like_source;
};

template <typename T, typename Source>
using mirror_cv_t = typename mirror_cv<T, Source>::type;


static_assert(std::is_same_v<mirror_cv_t<float,                  struct Source>, float>, "");
static_assert(std::is_same_v<mirror_cv_t<const float,            struct Source>, float>, "");
static_assert(std::is_same_v<mirror_cv_t<volatile float,         struct Source>, float>, "");
static_assert(std::is_same_v<mirror_cv_t<const volatile float,   struct Source>, float>, "");


static_assert(std::is_same_v<mirror_cv_t<float,                  const struct Source>, const float>, "");
static_assert(std::is_same_v<mirror_cv_t<const float,            const struct Source>, const float>, "");
static_assert(std::is_same_v<mirror_cv_t<volatile float,         const struct Source>, const float>, "");
static_assert(std::is_same_v<mirror_cv_t<const volatile float,   const struct Source>, const float>, "");


static_assert(std::is_same_v<mirror_cv_t<float,                  volatile struct Source>, volatile float>, "");
static_assert(std::is_same_v<mirror_cv_t<const float,            volatile struct Source>, volatile float>, "");
static_assert(std::is_same_v<mirror_cv_t<volatile float,         volatile struct Source>, volatile float>, "");
static_assert(std::is_same_v<mirror_cv_t<const volatile float,   volatile struct Source>, volatile float>, "");


static_assert(std::is_same_v<mirror_cv_t<float,                  const volatile struct Source>, const volatile float>, "");
static_assert(std::is_same_v<mirror_cv_t<const float,            const volatile struct Source>, const volatile float>, "");
static_assert(std::is_same_v<mirror_cv_t<volatile float,         const volatile struct Source>, const volatile float>, "");
static_assert(std::is_same_v<mirror_cv_t<const volatile float,   const volatile struct Source>, const volatile float>, "");

#endif

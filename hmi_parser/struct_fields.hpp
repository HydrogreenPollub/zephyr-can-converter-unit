#ifndef STRUCT_FIELDS_HPP
#define STRUCT_FIELDS_HPP

#include <utility>
#include <array>

#include "utils.hpp"
#include "indexed_type_map.hpp"



namespace detail{


    template<std::size_t I>
    struct UI{
        constexpr UI() noexcept = default;

        template <typename FieldType>
        constexpr operator FieldType() const noexcept { return FieldType();}
    };

    template <typename Struct, std::size_t I, std::size_t... Is>
    constexpr auto fieldCount(std::size_t& count, std::index_sequence<I, Is...>) -> decltype(Struct{UI<I>(), UI<Is>()...})
    {
        count = sizeof...(Is) + 1;
        return Struct{UI<I>(), UI<Is>()...};
    }


    template <typename Struct, std::size_t... Is>
    constexpr auto fieldCount(std::size_t& count, std::index_sequence<Is...>) -> void
    {
        fieldCount<Struct>(count, std::make_index_sequence<sizeof...(Is) - 1>{});
    }

} // namespace detail



template<typename Struct>
constexpr std::size_t getFieldCount()
{
    std::size_t count = 0;

    detail::fieldCount<Struct>(count, std::make_index_sequence<sizeof(Struct)>{});

    return count;
}



namespace detail{

    struct FieldInfo{
        std::size_t alignment;
        std::size_t size;
        std::size_t typeIndex;
        std::uintptr_t offset;
    };


    template<typename TypeMap, std::size_t FieldCount, std::size_t I>
    struct GetSizeAndAlignment{

        explicit constexpr GetSizeAndAlignment(std::array<FieldInfo, FieldCount>& fields) noexcept: _fields(fields) {}

        template <typename FieldType>
        constexpr operator FieldType() const noexcept
        {
            _fields[I].alignment = alignof(FieldType);
            _fields[I].size = sizeof(FieldType);
            _fields[I].typeIndex = IndexOfType<TypeMap, FieldType>;
            return FieldType();
        }

    private:
        std::array<FieldInfo, FieldCount>& _fields;
    };



    template <typename TypeMap, typename Struct, std::size_t FieldCount, std::size_t... Is>
    constexpr void getSizesAndAlignments(std::array<FieldInfo, FieldCount>& fields, std::index_sequence<Is...>)
    {
        [[maybe_unused]] Struct s{GetSizeAndAlignment<TypeMap, FieldCount, Is>(fields)...};
    }




    template <typename Struct, typename TypeMap = FundamentalTypes>
    constexpr auto getFieldInfos() -> std::array<FieldInfo, getFieldCount<Struct>()>
    {
        constexpr std::size_t fieldCount = getFieldCount<Struct>();

        std::array<FieldInfo, fieldCount> fields = {};

        getSizesAndAlignments<TypeMap, Struct, fieldCount>(fields, std::make_index_sequence<fieldCount>{});

        std::uintptr_t address = 0;
    
        for(auto& field: fields){

            std::uintptr_t misalignment = address % field.alignment;

            if (misalignment)
                address += (field.alignment - misalignment);

            field.offset = address;

            address += field.size;
        }

        return fields;
    }


    template <typename Struct, typename TypeMap, std::size_t... Is>
    constexpr auto implStructToTuple(std::index_sequence<Is...>)
    {
        constexpr auto fields = getFieldInfos<Struct, TypeMap>();

        return std::tuple<TypeAtIndex<TypeMap, fields[Is].typeIndex>...>{};
    }


    template <typename Struct, typename TypeMap = FundamentalTypes>
    constexpr auto structToTuple()
    {
        return implStructToTuple<Struct, TypeMap>(std::make_index_sequence<getFieldCount<Struct>()>{});
    }


} // namespace detail


template <std::size_t I, typename Struct, typename TypeMap = FundamentalTypes>
constexpr decltype(auto) getField(Struct& s)
{
    constexpr auto fields = detail::getFieldInfos<Struct, TypeMap>();

    using VoidPtr       = mirror_cv_t<void, Struct> *;
    using CharPtr       = mirror_cv_t<char, Struct> *;
    using FieldTypePtr  = mirror_cv_t<TypeAtIndex<TypeMap, fields[I].typeIndex>, Struct> *;

    return *static_cast<FieldTypePtr>(static_cast<VoidPtr>(static_cast<CharPtr>(static_cast<VoidPtr>(&s)) + fields[I].offset));
}


namespace detail{
    template <typename Struct, typename TypeMap, typename Func, std::size_t... Is>
    constexpr void implApplyToFields(Struct& s, Func func, std::index_sequence<Is...>)
    {
        (..., func(getField<Is>(s)));
    }
} // namespace detail

template <typename Struct, typename Func, typename TypeMap = FundamentalTypes>
constexpr void applyToFields(Struct& s, Func func)
{
    detail::implApplyToFields<Struct, TypeMap>(s, func, std::make_index_sequence<getFieldCount<Struct>()>{});
}

template <typename Struct, typename TypeMap = FundamentalTypes>
constexpr std::size_t sizeOfFields()
{
    constexpr auto fields = detail::getFieldInfos<Struct, TypeMap>();

    std::size_t totalSize = 0;

    for (std::size_t i=0; i<fields.size(); ++i)
        totalSize += fields[i].size;

    return totalSize;
}


#endif

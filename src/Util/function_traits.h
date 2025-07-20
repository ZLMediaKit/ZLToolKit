﻿#ifndef SRC_UTIL_FUNCTION_TRAITS_H_
#define SRC_UTIL_FUNCTION_TRAITS_H_

#include <tuple>
#include <functional>

namespace toolkit {

template<typename T>
struct function_traits;

//普通函数  [AUTO-TRANSLATED:569a9de3]
//Ordinary function
template<typename Ret, typename... Args>
struct function_traits<Ret(Args...)>
{
public:
    static constexpr size_t arity = sizeof...(Args);
    using function_type = Ret(Args...);
    using return_type = Ret;
    using stl_function_type = std::function<function_type>;
    using pointer = Ret(*)(Args...);

    template<size_t I>
    struct args
    {
        static_assert(I < arity, "index is out of range, index must less than sizeof Args");
        using type = typename std::tuple_element<I, std::tuple<Args...> >::type;
    };
};

//函数指针  [AUTO-TRANSLATED:bc15033e]
//Function pointer
template<typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> : function_traits<Ret(Args...)>{};

//std::function
template <typename Ret, typename... Args>
struct function_traits<std::function<Ret(Args...)>> : function_traits<Ret(Args...)>{};

//member function
#define FUNCTION_TRAITS(...) \
    template <typename ReturnType, typename ClassType, typename... Args>\
    struct function_traits<ReturnType(ClassType::*)(Args...) __VA_ARGS__> : function_traits<ReturnType(Args...)>{}; \

FUNCTION_TRAITS()
FUNCTION_TRAITS(const)
FUNCTION_TRAITS(volatile)
FUNCTION_TRAITS(const volatile)

//函数对象  [AUTO-TRANSLATED:a0091563]
//Function object
template<typename Callable>
struct function_traits : function_traits<decltype(&Callable::operator())>{};

} /* namespace toolkit */

#endif /* SRC_UTIL_FUNCTION_TRAITS_H_ */

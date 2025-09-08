#pragma once


#include <type_traits> // For std::true_type, std::false_type

template <typename T, typename U = T>
struct HasOperatorEquals
{
private:

	template <typename V, typename W>
	static auto test(int) -> decltype(std::declval<V>() == std::declval<W>(), std::true_type());

	// Fallback for types that don't have the operator
	template <typename, typename>
	static auto test(...) -> std::false_type;

public:
	static constexpr bool value = decltype(test<T, U>(0))::value;
};

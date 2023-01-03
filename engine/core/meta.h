#pragma once

template <int... Is>
struct index_list {};

template <int MIN, int N, int... Is>
struct range_builder;

template <int MIN, int... Is>
struct range_builder<MIN, MIN, Is...> {
	using type = index_list<Is...>;
};

template <int MIN, int N, int... Is>
struct range_builder : public range_builder<MIN, N-1, N-1, Is...> {};

template <int MIN, int MAX>
using index_range = typename range_builder<MIN, MAX>::type;

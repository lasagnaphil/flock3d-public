#include "core/random.h"
#include "glm/detail/qualifier.hpp"

#include <random>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <type_traits>

// Global device that generates seeds
std::random_device g_random_device;

// Thread local random engine.
thread_local std::mt19937 tl_random_engine;

void random_thread_local_seed() {
	tl_random_engine = std::mt19937(g_random_device());
}

template <class K>
struct is_glm_vec : std::false_type {};

template <glm::length_t L, class T, glm::qualifier Q>
struct is_glm_vec<glm::vec<L, T, Q>> : std::true_type {
	static constexpr glm::length_t len = L;
	using item_type = T;
};

template <class T>
T random_uniform(T min, T max) {
	if constexpr (std::is_arithmetic_v<T>) {
		if constexpr (std::is_integral_v<T>) {
			std::uniform_int_distribution<T> dist(min, max);
			return dist(tl_random_engine);
		}
		else if constexpr (std::is_floating_point_v<T>) {
			std::uniform_real_distribution<T> dist(min, max);
			return dist(tl_random_engine);
		}
		else {
			return {};
		}
	}
	else if constexpr (is_glm_vec<T>::value) {
		using item_type = typename is_glm_vec<T>::item_type;
		constexpr glm::length_t len = is_glm_vec<T>::len;

		glm::vec<len, item_type> vec;
		for (int i = 0; i < len; i++) {
			vec[i] = random_uniform<item_type>(min[i], max[i]);
		}
		return vec;
	}
	else {
		return {};
	}
}

template <class T>
T random_normal(T mean, T std) {
	if constexpr (std::is_floating_point_v<T>) {
		std::normal_distribution<T> dist(mean, std);
		return dist(tl_random_engine);
	}
	else if constexpr (is_glm_vec<T>::value) {
		using item_type = typename is_glm_vec<T>::item_type;
		constexpr glm::length_t len = is_glm_vec<T>::len;

		glm::vec<len, item_type> vec;
		for (int i = 0; i < len; i++) {
			vec[i] = random_normal<item_type>(mean[i], std[i]);
		}
		return vec;
	}
	else {
		return {};
	}
}

#define INSTANTIATE_UNIFORM(T) \
template T random_uniform<T>(T min, T max);

#define INSTANTIATE_NORMAL(T) \
template T random_normal<T>(T mean, T std);

INSTANTIATE_UNIFORM(int32_t)
INSTANTIATE_UNIFORM(int64_t)
INSTANTIATE_UNIFORM(uint32_t)
INSTANTIATE_UNIFORM(uint64_t)
INSTANTIATE_UNIFORM(float)
INSTANTIATE_UNIFORM(double)
INSTANTIATE_UNIFORM(glm::ivec2)
INSTANTIATE_UNIFORM(glm::ivec3)
INSTANTIATE_UNIFORM(glm::ivec4)
INSTANTIATE_UNIFORM(glm::vec2)
INSTANTIATE_UNIFORM(glm::vec3)
INSTANTIATE_UNIFORM(glm::vec4)

INSTANTIATE_NORMAL(float)
INSTANTIATE_NORMAL(double)
INSTANTIATE_NORMAL(glm::vec2)
INSTANTIATE_NORMAL(glm::vec3)
INSTANTIATE_NORMAL(glm::vec4)

#undef INSTANTIATE_UNI
#undef INSTANTIATE_NORMAL

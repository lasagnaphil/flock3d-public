#pragma once

#include "core/log.h"
#include "core/span.h"

template <class T>
T min(const T* data, uint32_t len) {
	log_assert(len != 0);
	T min = data[0];
	for (uint32_t i = 1; i < len; i++) {
		if (data[i] < min) min = data[i];
	}
	return min;
}

template <class T>
T max(const T* data, uint32_t len) {
	log_assert(len != 0);
	T max = data[0];
	for (uint32_t i = 1; i < len; i++) {
		if (data[i] > max) max = data[i];
	}
	return max;
}

template <class T>
T min(Span<T> span) {
	return min(span.data(), span.size());
}

template <class T>
T max(Span<T> span) {
	return max(span.data(), span.size());
}
#pragma once

#include <type_traits>

#include "span.h"
#include "log.h"

template <class T, uint32_t _Size>
class Array {
public:
    T _data[_Size];

    constexpr uint32_t size() const { return _Size; }

    const T* data() const { return _data; }
    T* data() { return _data; }

    const T* begin() const { return _data; }
    T* begin() { return _data; }

    const T* end() const { return _data + _Size; }
    T* end() { return _data + _Size; }

    constexpr const T& operator[](uint32_t i) const { 
        log_assert(i < _Size);
        return _data[i]; 
    };
    constexpr T& operator[](uint32_t i) { 
        log_assert(i < _Size);
        return _data[i]; 
    };

    constexpr operator Span<T>() { return {_data, _Size}; }
};

// Template deduction rules for Array. 
// A really obscure part of C++ that almost no one really knows...
template <class T, class... U>
Array(T, U...) -> Array<T, 1 + sizeof...(U)>;

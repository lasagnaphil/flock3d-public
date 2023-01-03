#pragma once

#include <stdint.h>

template <class T>
class Span {
    T* _data;
    uint32_t _size;

public:
    Span(T* data = nullptr, uint32_t size = 0) : _data(data), _size(size) {}

    const T* data() const { return _data; }
    T* data() { return _data; }

    const T* begin() const { return _data; }
    T* begin() { return _data; }
    const T* end() const { return _data + _size; }
    T* end() { return _data + _size; }

    const T& operator[](uint32_t i) const { return _data[i]; }
    T& operator[](uint32_t i) { return _data[i]; }

    uint32_t size() const { return _size; }
    int32_t ssize() const { return (int32_t)_size; }
};
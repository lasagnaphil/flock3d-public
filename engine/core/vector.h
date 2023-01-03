#pragma once

#define MAX(a, b) (((a)>(b))? (a):(b))

#include <type_traits>

#include "span.h"

template <class T>
class Vector {
    uint32_t _capacity, _size;
    T* _data = nullptr;

public:
    Vector(uint32_t size = 0) : _capacity(size), _size(size), _data(size ? new T[size] : nullptr) {}

    Vector(uint32_t size, const T& item) {
        for (uint32_t i = 0; i < _size; i++) {
            _data[i] = item;
        }
    }

    ~Vector() {
        if (_data) {
            delete[] _data;
        }
    }

    Vector(const Vector& other) : _capacity(other._capacity), _size(other._size),
        _data(other._capacity? new T[other._capacity] : nullptr)
    {
        copy(other._data, other._size, _data);
    }

    friend void swap(Vector& first, Vector& second) noexcept {
        using std::swap;

        swap(first._capacity, second._capacity);
        swap(first._size, second._size);
        swap(first._data, second._data);
    }

    Vector(Vector&& other) noexcept {
        swap(*this, other);
    }

    Vector& operator=(Vector other) {
        swap(*this, other);
        return *this;
    }

    Vector(std::initializer_list<T> lst) : _capacity(lst.size()), _size(lst.size()),
        _data(lst.size()? new T[lst.size()] : nullptr)
    {
        copy(std::data(lst), lst.size(), _data);
    }

    operator Span<T>() const {
        return {_data, _size};
    }

    uint32_t size() const { return _size; }
    int32_t ssize() const { return (int32_t)_size; }

    uint32_t capacity() const { return _capacity; }

    const T* data() const { return _data; }
    T* data() { return _data; }

    const T* begin() const { return _data; }
    T* begin() { return _data; }

    const T* end() const { return _data + _size; }
    T* end() { return _data + _size; }

    const T& operator[](uint32_t i) const { return _data[i]; };
    T& operator[](uint32_t i) { return _data[i]; }

    const T& front() const { return _data[0]; }
    T& front() { return _data[0]; }

    const T& back() const { return _data[_size-1]; }
    T& back() { return _data[_size-1]; }

    bool empty() const { return _size == 0; }

    void push_back(T elem) {
        ensure_capacity(_size + 1);
        _data[_size++] = elem;
    }

    T& push_empty() {
        ensure_capacity(_size + 1);
        return _data[_size++];
    }

    template <class ...Args>
    void emplace_back(Args&&... args) {
        ensure_capacity(_size + 1);
        new (_data + _size++) T(args...);
    }

    T pop_back() {
        return _data[--_size];
    }

    void resize(uint32_t new_size) {
        T* new_data = new T[new_size];
        copy(_data, _size, new_data);
        delete[] _data;
        _data = new_data;
        _capacity = _size = new_size;
    }

    void reserve(uint32_t new_capacity) {
        if (new_capacity <= _capacity) return;
        T* new_data = new T[new_capacity];
        copy(_data, _size, new_data);
        delete[] _data;
        _data = new_data;
        _capacity = new_capacity;
    }

    void clear() {
        if (_data) {
            delete[] _data;
            _data = nullptr;
        }
        _capacity = _size = 0;
    }

    T* find(const T& item) {
        for (uint32_t i = 0; i < _size; i++) {
            if (_data[i] == item) return &_data[i];
        }
        return nullptr;
    }

    template <class Predicate>
    T* find_if(Predicate&& predicate) {
        for (uint32_t i = 0; i < _size; i++) {
            if (predicate(_data[i])) return &_data[i];
        }
        return nullptr;
    }

private:
    static void copy(const T* src, uint32_t n, T* dst) {
        if constexpr (std::is_trivially_copyable<T>::value) {
            memcpy(dst, src, sizeof(T) * n);
        }
        else {
            for (uint32_t i = 0; i < n; i++) {
                dst[i] = src[i];
            }
        }
    }

    void ensure_capacity(uint32_t min_capacity) {
        if (min_capacity <= _capacity) return;
        size_t new_capacity = MAX(_capacity * 2, min_capacity);
        T* new_data = new T[new_capacity];
        copy(_data, _size, new_data);
        delete[] _data;
        _data = new_data;
        _capacity = new_capacity;
    }
};

#undef MAX
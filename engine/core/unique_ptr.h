#pragma once

// A very lightweight unique_ptr class that doesn't include a bunch of STL headers.
template <class T>
class UniquePtr {
private:
	T* _data;
public:
	UniquePtr() : _data(nullptr) {}
	UniquePtr(const UniquePtr&) = delete;
	UniquePtr& operator=(const UniquePtr&) = delete;
	UniquePtr(T* data) : _data(data) {}
	UniquePtr(UniquePtr&& other) : _data(other._data) {
		other._data = nullptr;
	}
	~UniquePtr() { reset(); }

    UniquePtr &operator=(UniquePtr&& other) {
        reset();
        _data = other._data;
        other._data = nullptr;
        return *this;
    }

	void reset() noexcept(true) {
		delete _data;
		_data = nullptr;
	}

	T* get() { return _data; }
	const T* get() const { return _data; }
	T* operator->() { return _data; }
	const T* operator->() const { return _data; }

	T& operator*() { return *_data; }
	const T& operator*() const { return *_data; }

	T* release() {
		T* tmp = _data;
		_data = nullptr;
		return tmp;
	}
};
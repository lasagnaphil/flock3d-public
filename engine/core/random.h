// Utility header for generating random values
// This is a wrapper of the C++ <random> header, which I find it too verbose and bloated to use in practice.

#pragma once

void random_thread_local_seed();

template <class T>
T random_uniform(T min, T max);

template <class T>
T random_normal(T mean, T std);
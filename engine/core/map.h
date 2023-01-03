#pragma once

#include <parallel_hashmap/phmap.h>

template <class Key, class Value, class Hash = phmap::priv::hash_default_hash<Key>>
using Map = typename phmap::flat_hash_map<Key, Value, Hash>;

template <class Key, class Value, class Hash = phmap::priv::hash_default_hash<Key>>
using StableMap = typename phmap::node_hash_map<Key, Value, Hash>;

template <class Key, class Value, class Hash = phmap::priv::hash_default_hash<Key>>
using ParallelMap = typename phmap::parallel_flat_hash_map<Key, Value, Hash>;

template <class Key, class Value, class Hash = phmap::priv::hash_default_hash<Key>>
using ParallelStableMap = typename phmap::parallel_node_hash_map<Key, Value, Hash>;

#pragma once

#include <parallel_hashmap/phmap.h>

template <class Key, class Hash = phmap::priv::hash_default_hash<Key>>
using Set = phmap::flat_hash_set<Key, Hash>;

template <class Key, class Hash = phmap::priv::hash_default_hash<Key>>
using StableSet = phmap::node_hash_set<Key, Hash>;

template <class Key, class Hash = phmap::priv::hash_default_hash<Key>>
using ParallelSet = phmap::parallel_flat_hash_set<Key, Hash>;

template <class Key, class Hash = phmap::priv::hash_default_hash<Key>>
using ParallelStableSet = phmap::parallel_node_hash_set<Key, Hash>;

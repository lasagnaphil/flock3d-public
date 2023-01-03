#pragma once

#include "gen_arena.h"

using GenArenaStorageConfig = GenArenaConfig<32, 8, 24>;

template <class T>
using Storage = GenArena<T, GenArenaStorageConfig>;

template <class T>
using Ref = GenArenaTypedRef<T, GenArenaStorageConfig>;